#!/usr/bin/env python3
"""Verify model selection applies context limits to the child, including same-model changes."""
import argparse
import json
import os
import pathlib
import shutil
import socket
import subprocess
import struct
import sys
import tempfile
import time
import urllib.request

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--binary', required=True)
args = parser.parse_args()

def free_port():
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]

def verify_fixture_concurrency(worker):
    """An idle connection must not block the wrapper's concurrent health probes."""
    port = free_port()
    child = subprocess.Popen([str(worker), '-c', '2048', '--port', str(port)],
                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    idle = None
    try:
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            try:
                idle = socket.create_connection(('127.0.0.1', port), timeout=1)
                break
            except OSError:
                time.sleep(.05)
        assert idle is not None, 'Context fixture did not start'
        with urllib.request.urlopen(f'http://127.0.0.1:{port}/health', timeout=1) as response:
            assert response.status == 200
    finally:
        if idle is not None:
            idle.close()
        child.terminate()
        child.wait(timeout=5)

with tempfile.TemporaryDirectory(prefix='delta-wrapper-context-') as work:
    root = pathlib.Path(work)
    binary = root / 'delta-server'
    shutil.copy2(pathlib.Path(args.binary).resolve(), binary)
    worker = root / 'llama-server'
    worker.write_text('#!' + sys.executable + '\n' + '''import http.server,json,pathlib,sys
args=sys.argv[1:]
ctx=int(args[args.index('-c')+1]) if '-c' in args else 131072
port=int(args[args.index('--port')+1])
with pathlib.Path(__file__).with_name('launches.jsonl').open('a') as out:
 out.write(json.dumps({'ctx':ctx,'args':args})+'\\n')
class Handler(http.server.BaseHTTPRequestHandler):
 def do_GET(self):
  body=json.dumps({'status':'ok','total_slots':1,'model_alias':'MiniCPM5-1B-Q6_K','default_generation_settings':{'n_ctx':ctx},'data':[{'id':'MiniCPM5-1B-Q6_K','status':{'value':'loaded'}}]}).encode()
  self.send_response(200);self.send_header('Content-Type','application/json');self.end_headers();self.wfile.write(body)
 def do_POST(self):
  self.rfile.read(int(self.headers.get('Content-Length',0)))
  self.send_response(200);self.end_headers();self.wfile.write(b'{"success":true}')
 def log_message(self,*args):pass
http.server.ThreadingHTTPServer(('127.0.0.1',port),Handler).serve_forever()
''')
    worker.chmod(0o755)
    verify_fixture_concurrency(worker)
    (root / 'launches.jsonl').unlink()
    home = root / 'home'
    models = home / '.delta-cli' / 'models'
    models.mkdir(parents=True)
    (models / 'MiniCPM5-1B-Q6_K.gguf').write_bytes(b'GGUF'+struct.pack('<IQQ',3,1,0)+bytes(160))
    llama_port, api_port = free_port(), free_port()
    env = {'PATH': os.defpath, 'HOME': str(home), 'XDG_DATA_HOME': str(home / 'data')}
    with (root / 'wrapper.log').open('wb') as log:
        process = subprocess.Popen([str(binary), '--models-dir', str(models), '--port', str(llama_port),
                                    '--model-api-port', str(api_port), '--parallel', '1'], cwd=root, env=env,
                                   stdout=log, stderr=subprocess.STDOUT)
        def read_context():
            with urllib.request.urlopen(f'http://127.0.0.1:{llama_port}/props', timeout=1) as response:
                return json.load(response)['default_generation_settings']['n_ctx']
        try:
            deadline = time.monotonic() + 20
            while time.monotonic() < deadline:
                try:
                    read_context()
                    with urllib.request.urlopen(f'http://127.0.0.1:{api_port}/api/models/list', timeout=1):
                        break
                except OSError:
                    time.sleep(.1)
            else:
                raise RuntimeError('Wrapper did not start: ' + (root / 'wrapper.log').read_text(errors='replace')[-4000:])
            for requested, expected, persist in [(None,8192,True),(4096,4096,True),(None,4096,True),(2048,2048,False),(None,4096,True)]:
                body = {'model': 'MiniCPM5:1b', 'persist_ctx':persist}
                if requested is not None:
                    body['ctx_size'] = requested
                request = urllib.request.Request(f'http://127.0.0.1:{api_port}/api/models/use',
                                                 data=json.dumps(body).encode(),
                                                 headers={'Content-Type': 'application/json'})
                with urllib.request.urlopen(request, timeout=30) as response:
                    result = json.load(response)
                assert result['ctx_size'] == expected, result
                deadline=time.monotonic()+20
                while time.monotonic()<deadline:
                    with urllib.request.urlopen(f'http://127.0.0.1:{api_port}/api/models/runtime?attempt_id='+result['attempt_id'],timeout=2) as response:
                        snapshot=json.load(response)
                    if snapshot['state']=='ready':break
                    assert snapshot['state']=='loading',snapshot
                    time.sleep(.1)
                else:raise AssertionError('Context selection did not become ready: '+json.dumps(snapshot))
                actual = read_context()
                assert actual == expected, f'API reported {expected}, but child actually uses {actual}'
            launches = [json.loads(line) for line in (root / 'launches.jsonl').read_text().splitlines()]
            assert [launch['ctx'] for launch in launches][-5:] == [8192,4096,4096,2048,4096], launches
            assert all('--models-dir' in launch['args'] for launch in launches), launches
            print('Fresh selection and same-model context changes reach the actual child process')
        except Exception:
            print((root / 'wrapper.log').read_text(errors='replace')[-6000:], file=sys.stderr)
            launches = root / 'launches.jsonl'
            if launches.exists():
                print('Worker launches: ' + launches.read_text(), file=sys.stderr)
            raise
        finally:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
