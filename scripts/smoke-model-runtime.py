#!/usr/bin/env python3
"""Load a real GGUF and generate tokens using the release binary in isolation."""
import argparse
import json
import os
import pathlib
import socket
import subprocess
import tempfile
import time
import urllib.request

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--binary', required=True)
parser.add_argument('--model', required=True)
parser.add_argument('--qemu', action='store_true')
args = parser.parse_args()
with socket.socket() as sock:
    sock.bind(('127.0.0.1', 0))
    port = sock.getsockname()[1]
command = ['qemu-x86_64', '-cpu', 'qemu64'] if args.qemu else []
command += [str(pathlib.Path(args.binary).resolve()), '-m', str(pathlib.Path(args.model).resolve()),
            '-c', '512', '-ngl', '0', '--host', '127.0.0.1', '--port', str(port)]
with tempfile.TemporaryDirectory(prefix='delta-runtime-smoke-') as work:
    log = pathlib.Path(work) / 'backend.log'
    with log.open('wb') as output:
        process = subprocess.Popen(command, cwd=work, stdout=output, stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic() + 120
            while time.monotonic() < deadline:
                if process.poll() is not None:
                    raise RuntimeError(f'Backend exited with {process.returncode}: {log.read_text(errors="replace")[-6000:]}')
                try:
                    with urllib.request.urlopen(f'http://127.0.0.1:{port}/health', timeout=1) as response:
                        if response.status == 200:
                            break
                except OSError:
                    time.sleep(0.2)
            else:
                raise RuntimeError('Backend did not become ready within 120 seconds')
            request = urllib.request.Request(f'http://127.0.0.1:{port}/completion',
                data=json.dumps({'prompt': 'Once upon a time', 'n_predict': 4, 'temperature': 0}).encode(),
                headers={'Content-Type': 'application/json'})
            with urllib.request.urlopen(request, timeout=60) as response:
                result = json.load(response)
            if not result.get('content') or result.get('tokens_predicted', 0) < 1:
                raise RuntimeError(f'Backend produced no tokens: {result}')
            print('Packaged backend loaded GGUF and generated tokens' + (' without AVX under QEMU' if args.qemu else ''))
        finally:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
