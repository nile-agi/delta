#!/usr/bin/env python3
"""Model selection must verify the worker rather than the idle router."""
import argparse
import json
import pathlib
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import time
import urllib.request
import urllib.error

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--binary', required=True)
args = parser.parse_args()

def port():
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]

with tempfile.TemporaryDirectory(prefix='delta-readiness-') as directory:
    root = pathlib.Path(directory)
    binary = root / 'delta-server'
    shutil.copy2(pathlib.Path(args.binary).resolve(), binary)
    worker = root / 'llama-server'
    worker.write_text('#!' + sys.executable + '\n' + '''import http.server,json,pathlib,sys,urllib.parse
args=sys.argv[1:]; root=pathlib.Path(__file__).parent
ctx=int(args[args.index('-c')+1]) if '-c' in args else 131072
port=int(args[args.index('--port')+1]); loaded=False
alias='MiniCPM5-1B-Q6_K'
class Handler(http.server.BaseHTTPRequestHandler):
 def do_GET(self):
  mode=(root/'mode').read_text()
  path=urllib.parse.urlsplit(self.path).path
  if path in ['/models','/v1/models']:
   data={'data':[{'id':alias if mode!='alias' else 'wrong-alias','status':{'value':'loaded' if loaded else 'unloaded'}}]}
  elif path=='/props':
   data={'model_path':alias+'.gguf','total_slots':1,'default_generation_settings':{'n_ctx':ctx+(1024 if mode=='context' else 0)}}
   if mode=='props':data={}
   if mode=='json':
    self.send_response(200);self.end_headers();self.wfile.write(b'invalid json');return
  else:data={'status':'ok'}
  self.send_response(200);self.end_headers();self.wfile.write(json.dumps(data).encode())
 def do_POST(self):
  global loaded
  body=json.loads(self.rfile.read(int(self.headers.get('Content-Length',0))) or '{}')
  if self.path=='/models/load':
   with (root/'loads').open('a') as out:out.write(json.dumps(body)+'\\n')
   loaded=True
  self.send_response(200);self.end_headers();self.wfile.write(b'{"success":true}')
 def log_message(self,*args):pass
http.server.ThreadingHTTPServer(('127.0.0.1',port),Handler).serve_forever()
''')
    worker.chmod(0o755)
    home = root / 'home'
    models = home / '.delta-cli/models'
    models.mkdir(parents=True)
    # A bounded GGUF header plus one tensor record; the stand-in does not read weights.
    (models / 'MiniCPM5-1B-Q6_K.gguf').write_bytes(
        b'GGUF' + struct.pack('<IQQ', 3, 1, 0) + struct.pack('<Q', 1) + b'x' +
        struct.pack('<IQIQ', 1, 32, 0, 0) + bytes(128))
    (root / 'mode').write_text('ok')
    engine_port, api_port = port(), port()
    import os
    env = {'PATH': os.defpath, 'HOME': str(home), 'LLAMA_ARG_N_PARALLEL': '1'}
    with (root / 'log').open('wb') as log:
        child = subprocess.Popen([str(binary), '--models-dir', str(models), '--parallel', '1',
                                  '--port', str(engine_port), '--model-api-port', str(api_port)],
                                 env=env, cwd=root, stdout=log, stderr=subprocess.STDOUT)
        def call(path, body=None):
            request = urllib.request.Request(f'http://127.0.0.1:{api_port}' + path,
                                             data=None if body is None else json.dumps(body).encode(),
                                             headers={'Content-Type': 'application/json'})
            with urllib.request.urlopen(request, timeout=5) as response:
                return json.load(response)
        def wait_ready(attempt):
            deadline = time.monotonic() + 12
            while time.monotonic() < deadline:
                snapshot = call('/api/models/runtime?attempt_id=' + attempt)
                if snapshot['state'] != 'loading': return snapshot
                time.sleep(.05)
            raise AssertionError('Attempt did not reach terminal state')
        try:
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline:
                try:
                    call('/api/models/list')
                    break
                except OSError: time.sleep(.1)
            for mode in ['ok', 'context', 'props', 'json', 'alias']:
                (root / 'mode').write_text(mode)
                response = call('/api/models/use', {'model': 'MiniCPM5:1b', 'ctx_size': 2048})
                assert response['loaded'] is False, f'Idle router was advertised as loaded: {response}'
                assert response['state'] == 'loading' and response['attempt_id'], response
                snapshot = wait_ready(response['attempt_id'])
                assert snapshot['state'] == ('ready' if mode == 'ok' else 'failed'), snapshot
                if mode == 'ok':
                    assert snapshot['effective_config']['context'] == 2048, snapshot
                    assert snapshot['effective_config']['parallel'] == 1, snapshot
            loads = [json.loads(line) for line in (root / 'loads').read_text().splitlines()]
            assert len(loads) == 4 and all(x['model'] == 'MiniCPM5-1B-Q6_K' for x in loads), loads
            try: call('/api/models/runtime?attempt_id=unknown')
            except urllib.error.HTTPError as error: assert error.code == 404
            else: raise AssertionError('Unknown attempt accepted')
            print('Worker readiness, exact alias/context, malformed props and attempt status checks passed')
        finally:
            child.terminate()
            try: child.wait(timeout=8)
            except subprocess.TimeoutExpired: child.kill();child.wait()
