#!/usr/bin/env python3
"""Run installed small models through Delta's complete selection/readiness/generation path."""
import argparse
import hashlib
import json
import os
import pathlib
import signal
import socket
import subprocess
import tempfile
import threading
import time
import urllib.error
import urllib.parse
import urllib.request

FILES = {'gemma3:270m': 'gemma-3-270m-it-Q8_0.gguf',
         'qwen2.5:0.5b': 'qwen2.5-0.5b-instruct-q4_k_m.gguf'}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', required=True, type=pathlib.Path)
    parser.add_argument('--model-dir', required=True, type=pathlib.Path)
    parser.add_argument('--model', required=True, choices=FILES)
    parser.add_argument('--output', required=True, type=pathlib.Path)
    parser.add_argument('--context', type=int, default=2048)
    parser.add_argument('--rss-limit-mib', type=int, default=2048)
    parser.add_argument('--library-dir', action='append', default=[], type=pathlib.Path,
                        help='Only explicitly approved extracted-package library directories')
    args = parser.parse_args()
    source = (args.model_dir / FILES[args.model]).resolve(strict=True)
    binary = args.binary.resolve(strict=True)
    original = source.stat()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    result = {'model': args.model, 'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
              'context': args.context, 'checks': {}, 'peak_rss_mib': 0}
    def free_port():
        with socket.socket() as sock:
            sock.bind(('127.0.0.1', 0)); return sock.getsockname()[1]
    port, api = free_port(), free_port()
    def request(path, body=None, engine=False, timeout=5):
        req = urllib.request.Request(f'http://127.0.0.1:{port if engine else api}' + path,
                                     data=None if body is None else json.dumps(body).encode(),
                                     headers={'Content-Type': 'application/json'})
        try:
            with urllib.request.urlopen(req, timeout=timeout) as response:
                return response.status, json.load(response)
        except urllib.error.HTTPError as error:
            return error.code, error.read(8192).decode(errors='replace')
    def select():
        status, accepted = request('/api/models/use', {'model': args.model, 'ctx_size': args.context})
        assert status == 200 and accepted['state'] == 'loading' and not accepted['loaded'], accepted
        return accepted
    def wait_terminal(attempt):
        deadline = time.monotonic() + 190
        while time.monotonic() < deadline:
            status, snapshot = request('/api/models/runtime?attempt_id=' + urllib.parse.quote(attempt))
            assert status == 200, snapshot
            if snapshot['state'] != 'loading': return snapshot
            time.sleep(.2)
        raise AssertionError('Load exceeded the backend deadline')
    with tempfile.TemporaryDirectory(prefix='delta-real-runtime-') as folder:
        profile = pathlib.Path(folder)
        models = profile / '.delta-cli/models'
        models.mkdir(parents=True)
        (models / FILES[args.model]).symlink_to(source)
        env = {'PATH': os.defpath, 'HOME': str(profile), 'XDG_DATA_HOME': str(profile / 'data'),
               'LLAMA_ARG_THREADS': '2', 'LLAMA_ARG_N_GPU_LAYERS': '0',
               'LLAMA_ARG_BATCH': '64', 'LLAMA_ARG_UBATCH': '64',
               'OMP_NUM_THREADS': '2', 'VECLIB_MAXIMUM_THREADS': '2'}
        if args.library_dir:
            env['LD_LIBRARY_PATH'] = ':'.join(str(path.resolve(strict=True)) for path in args.library_dir)
        stopped = threading.Event()
        owned = set()
        with args.output.with_suffix('.log').open('wb') as log:
            child = subprocess.Popen([str(binary), '--models-dir', str(models), '--parallel', '1',
                                      '--port', str(port), '--model-api-port', str(api)], cwd=profile,
                                     env=env, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
            owned.add(child.pid)
            def monitor():
                while not stopped.wait(.5):
                    try:
                        rows = [list(map(int, line.split())) for line in subprocess.check_output(
                            ['ps', '-axo', 'pid=,ppid=,rss='], text=True).splitlines()]
                        for _ in range(16):
                            found = {pid for pid, parent, rss in rows if parent in owned}
                            if found.issubset(owned): break
                            owned.update(found)
                        rss = sum(rss for pid, parent, rss in rows if pid in owned) / 1024
                        result['peak_rss_mib'] = max(result['peak_rss_mib'], round(rss, 1))
                        if rss <= args.rss_limit_mib: continue
                        result['memory_guard_triggered'] = True
                    except Exception as error:
                        result['monitor_error'] = str(error)
                    try: os.killpg(child.pid, signal.SIGTERM)
                    except ProcessLookupError: pass
                    return
            watcher = threading.Thread(target=monitor, daemon=True)
            watcher.start()
            try:
                deadline = time.monotonic() + 35
                while time.monotonic() < deadline:
                    if child.poll() is not None: raise AssertionError('Backend exited during startup')
                    try:
                        if request('/api/models/runtime')[0] == 200: break
                    except OSError: pass
                    time.sleep(.1)
                else: raise AssertionError('Model API unavailable after startup deadline')
                cancelled = select()
                status, snapshot = request('/api/models/runtime/cancel', {'attempt_id': cancelled['attempt_id']})
                assert status == 200 and snapshot['state'] == 'cancelled', snapshot
                result['checks']['cancelled_load'] = True
                start = time.monotonic()
                accepted = select()
                assert accepted['attempt_id'] != cancelled['attempt_id']
                snapshot = wait_terminal(accepted['attempt_id'])
                assert snapshot['state'] == 'ready' and snapshot['loaded'], snapshot
                result['load_seconds'] = round(time.monotonic() - start, 3)
                result['checks']['ready_without_manual_load'] = True
                result['effective_config'] = snapshot['effective_config']
                props_status, props = request('/props?model=' + urllib.parse.quote(accepted['model_alias']) + '&autoload=false', engine=True)
                assert props_status == 200 and props['total_slots'] == 1, props
                assert props['default_generation_settings']['n_ctx'] == args.context, props
                result['checks']['actual_context_and_slots'] = True
                start = time.monotonic()
                status, generation = request('/v1/chat/completions', {'model': args.model,
                    'messages': [{'role': 'user', 'content': 'Say hello in one short sentence.'}],
                    'max_tokens': 32, 'temperature': 0, 'stream': False, 'use_tools': False,
                    'use_documents': False, 'use_files_tools': False, 'use_shell_tools': False}, timeout=90)
                assert status == 200, generation
                text = generation['choices'][0]['message']['content']
                assert text.strip(), generation
                result.update(generation_seconds=round(time.monotonic() - start, 3), reply=text)
                result['checks']['generated_text'] = True
                assert request('/api/models/use', {'model': 'not-installed-test-model'})[0] == 404
                assert request('/health', engine=True)[0] == 200
                result['checks']['invalid_selection_preserves_engine'] = True
                assert not result.get('memory_guard_triggered') and not result.get('monitor_error')
            except Exception as error:
                result['error'] = str(error)
            finally:
                try: request('/api/models/unload', {}, timeout=4)
                except Exception: pass
                try: os.killpg(child.pid, signal.SIGTERM)
                except ProcessLookupError: pass
                try: child.wait(timeout=8)
                except subprocess.TimeoutExpired: os.killpg(child.pid, signal.SIGKILL); child.wait()
                stopped.set(); watcher.join(timeout=2)
                result['checks']['source_preserved'] = (source.stat().st_size, source.stat().st_mtime_ns) == (original.st_size, original.st_mtime_ns)
                rows = subprocess.check_output(['ps', '-axo', 'pid=,command='], text=True)
                result['checks']['owned_processes_cleaned'] = not any(folder in row and ('llama-server' in row or 'delta-server' in row) for row in rows.splitlines())
    args.output.write_text(json.dumps(result, indent=2))
    print(json.dumps(result), flush=True)
    return 1 if result.get('error') or not all(result['checks'].values()) else 0

if __name__ == '__main__':
    raise SystemExit(main())
