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
http.server.HTTPServer(('127.0.0.1',port),Handler).serve_forever()
''')
    worker.chmod(0o755)
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
            for requested, expected in [(None, 8192), (4096, 4096), (None, 4096)]:
                body = {'model': 'MiniCPM5:1b'}
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
                else:raise AssertionError('Context selection did not become ready')
                actual = read_context()
                assert actual == expected, f'API reported {expected}, but child actually uses {actual}'
            launches = [json.loads(line) for line in (root / 'launches.jsonl').read_text().splitlines()]
            assert [launch['ctx'] for launch in launches][-3:] == [8192,4096,4096], launches
            assert all('--models-dir' in launch['args'] for launch in launches), launches
            print('Fresh selection and same-model context changes reach the actual child process')
        finally:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
