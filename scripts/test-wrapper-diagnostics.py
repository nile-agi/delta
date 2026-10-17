#!/usr/bin/env python3
"""Exercise child-process failure reporting with the built Unix wrapper in isolation."""
import argparse
import os
import pathlib
import shutil
import socket
import subprocess
import tempfile
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--binary', required=True)
args = parser.parse_args()

def free_port():
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]

with tempfile.TemporaryDirectory(prefix='delta-wrapper-diagnostics-') as work:
    root = pathlib.Path(work)
    binary = root / 'delta-server'
    shutil.copy2(pathlib.Path(args.binary).resolve(), binary)
    worker = root / 'llama-server'
    worker.write_text('#!/bin/sh\nprintf "ggml_backend: error test allocation failure\\n" >&2\nexit 7\n')
    worker.chmod(0o755)
    (root / 'models').mkdir()
    (root / 'home').mkdir()
    env = {'PATH': os.defpath, 'HOME': str(root / 'home'), 'XDG_DATA_HOME': str(root / 'home' / 'data')}
    log = root / 'wrapper.log'
    with log.open('wb') as output:
        process = subprocess.Popen([str(binary), '--models-dir', str(root / 'models'),
                                    '--port', str(free_port()), '--model-api-port', str(free_port())],
                                   cwd=root, env=env, stdout=output, stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic() + 20
            while time.monotonic() < deadline:
                text = log.read_text(errors='replace')
                if '[llama-server-exit] code=7 signal=0' in text and '[llama-server] ggml_backend: error test allocation failure' in text:
                    print('Wrapper preserves child exit status and forwarded runtime errors')
                    break
                if process.poll() is not None:
                    raise RuntimeError('Wrapper exited before child diagnostics were captured: ' + text[-3000:])
                time.sleep(0.1)
            else:
                raise RuntimeError('Missing child exit/error diagnostics: ' + log.read_text(errors='replace')[-3000:])
        finally:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
