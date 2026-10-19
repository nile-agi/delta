#!/usr/bin/env python3
"""Verify final Linux packages in clean runtimes with pinned quantized models."""
import argparse
import hashlib
import json
import os
import pathlib
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import urllib.request

BUDGET = 800 * 1024 * 1024

def manifest_entries(path):
    entries = json.loads(path.read_text())
    if not isinstance(entries, list) or not entries: raise ValueError('Missing fixture manifest')
    if sum(entry['bytes'] for entry in entries) > BUDGET: raise ValueError('Fixture download budget exceeded')
    for entry in entries:
        if not re.fullmatch(r'https://huggingface\.co/[^/]+/[^/]+/resolve/[0-9a-f]{40}/[^/]+\.gguf', entry['url']):
            raise ValueError('Fixture URL must use a pinned official revision')
        if pathlib.Path(entry['filename']).name != entry['filename'] or entry['bytes'] <= 0:
            raise ValueError('Invalid fixture filename or size')
        if not re.fullmatch('[0-9a-f]{64}', entry['sha256']) or not entry['tensor_types'] or 0 in entry['tensor_types']:
            raise ValueError('Fixture needs a checksum and quantized tensor types')
    return entries

def tensor_types(path):
    size = path.stat().st_size
    with path.open('rb') as file:
        def read(fmt):
            raw = file.read(struct.calcsize('<' + fmt))
            if len(raw) != struct.calcsize('<' + fmt): raise ValueError('Truncated GGUF')
            return struct.unpack('<' + fmt, raw)[0]
        def skip(count):
            if count < 0 or file.tell() + count > size: raise ValueError('GGUF record exceeds file')
            file.seek(count, 1)
        def string(): skip(read('Q'))
        widths = {0:1,1:1,2:2,3:2,4:4,5:4,6:4,7:1,10:8,11:8,12:8}
        def value(kind):
            if kind == 8: string()
            elif kind == 9:
                item, count = read('I'), read('Q')
                if item in widths: skip(widths[item] * count)
                elif item == 8:
                    if count > size // 8: raise ValueError('Invalid GGUF array')
                    for _ in range(count): string()
                else: raise ValueError('Unsupported GGUF array type')
            elif kind in widths: skip(widths[kind])
            else: raise ValueError('Invalid GGUF metadata type')
        if file.read(4) != b'GGUF' or read('I') not in (2, 3): raise ValueError('Invalid GGUF header')
        tensors, metadata = read('Q'), read('Q')
        if tensors > size // 24 or metadata > size // 12: raise ValueError('Invalid GGUF counts')
        for _ in range(metadata): string(); value(read('I'))
        types = set()
        for _ in range(tensors):
            string(); dimensions = read('I')
            if dimensions > 4: raise ValueError('Invalid tensor dimensions')
            skip(8 * dimensions); types.add(read('I')); read('Q')
        return types

def validate_fixture(path, entry):
    if path.stat().st_size != entry['bytes']: raise ValueError('Fixture size mismatch')
    digest = hashlib.sha256()
    with path.open('rb') as file:
        for block in iter(lambda:file.read(1024 * 1024), b''): digest.update(block)
    if digest.hexdigest() != entry['sha256']: raise ValueError('Fixture checksum mismatch')
    if not set(entry['tensor_types']).issubset(tensor_types(path)):
        raise ValueError('Fixture does not contain required quantized tensor types')

def find_engines(root):
    root = root.resolve()
    found = []
    for name in ['delta-server', 'llama-server']:
        matches = [p for p in root.rglob('*') if p.is_file() and re.fullmatch(name + r'(?:-x86_64-unknown-linux-gnu)?', p.name)]
        if len(matches) != 1: raise ValueError('Missing or ambiguous packaged ' + name)
        resolved = matches[0].resolve()
        if not resolved.is_relative_to(root): raise ValueError('Packaged engine points outside the artifact')
        found.append(resolved)
    return found

def check_library_paths(root, environment):
    for path in environment.get('LD_LIBRARY_PATH', '').split(':'):
        if path and not pathlib.Path(path).resolve().is_relative_to(root.resolve()):
            raise ValueError('Library search path escapes extracted package')
    if environment.get('LD_PRELOAD'): raise ValueError('Preloaded libraries cannot validate a release package')

def verify_probe_result(result):
    required = ['cancelled_load', 'ready_without_manual_load', 'actual_context_and_slots', 'generated_text',
                'invalid_selection_preserves_engine', 'source_preserved', 'owned_processes_cleaned']
    if result.get('error') or not all(result.get('checks', {}).get(key) is True for key in required):
        raise ValueError('Packaged model runtime probe failed')

def download_fixtures(entries, cache):
    cache.mkdir(parents=True, exist_ok=True)
    for entry in entries:
        target = cache / entry['filename']
        if not target.exists():
            partial = target.with_suffix('.part')
            try:
                received = 0
                with urllib.request.urlopen(entry['url'], timeout=90) as response, partial.open('wb') as file:
                    while block := response.read(1024 * 1024):
                        received += len(block)
                        if received > entry['bytes']: raise ValueError('Fixture exceeds declared download size')
                        file.write(block)
                validate_fixture(partial, entry)
                partial.rename(target)
            finally: partial.unlink(missing_ok=True)
        validate_fixture(target, entry)

def check_inside(args, entries):
    args.output.parent.mkdir(parents=True, exist_ok=True)
    result = {'artifact':args.artifact.name, 'runtime':args.runtime, 'models':[]}
    try:
        with tempfile.TemporaryDirectory(prefix='delta-extracted-') as directory:
            root = pathlib.Path(directory)
            if args.artifact.suffix == '.deb':
                subprocess.run(['dpkg-deb', '-x', str(args.artifact), str(root)], check=True)
            elif args.artifact.suffix == '.AppImage':
                image = root / 'image.AppImage'
                shutil.copy2(args.artifact, image); image.chmod(0o755)
                subprocess.run([str(image), '--appimage-extract'], cwd=root, check=True, stdout=subprocess.DEVNULL)
                root = root / 'squashfs-root'
            else: raise ValueError('Expected a final .deb or .AppImage artifact')
            backend, llama = find_engines(root)
            environment = {'PATH':os.defpath}
            library_dirs = []
            if args.artifact.suffix == '.AppImage':
                library_dirs = [p for p in [root/'usr/lib',root/'usr/lib/x86_64-linux-gnu',root/'lib'] if p.is_dir()]
                environment['LD_LIBRARY_PATH'] = ':'.join(str(p) for p in library_dirs)
            check_library_paths(root, environment)
            result['dependencies'] = {}
            for binary in [backend, llama]:
                linked = subprocess.run(['ldd', str(binary)], env=environment, text=True, capture_output=True)
                text = linked.stdout + linked.stderr
                result['dependencies'][binary.name] = text
                if 'not found' in text or (linked.returncode and 'not a dynamic executable' not in text):
                    raise ValueError('Unresolved packaged dependency: ' + text)
            for entry in entries:
                validate_fixture(args.fixture_cache / entry['filename'], entry)
                output = args.output.parent / (args.artifact.name + '-' + entry['model'].replace(':','-') + '.json')
                command = [sys.executable, str(pathlib.Path(__file__).with_name('test-runtime-models.py')),
                           '--binary',str(backend),'--model-dir',str(args.fixture_cache),
                           '--model',entry['model'],'--output',str(output)]
                for folder in library_dirs: command += ['--library-dir',str(folder)]
                subprocess.run(command, env=environment, check=True)
                report = json.loads(output.read_text()); verify_probe_result(report)
                result['models'].append(report)
    except Exception as error:
        result['error'] = str(error)
        raise
    finally:
        args.output.write_text(json.dumps(result, indent=2))

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--artifact', required=True, type=pathlib.Path)
    parser.add_argument('--runtime', choices=['ubuntu:22.04','ubuntu:24.04','fedora:44',
                                             'linuxmintd/mint22.3-amd64:latest'], required=True)
    parser.add_argument('--fixtures-manifest', required=True, type=pathlib.Path)
    parser.add_argument('--fixture-cache', required=True, type=pathlib.Path)
    parser.add_argument('--output', required=True, type=pathlib.Path)
    parser.add_argument('--inside-container', action='store_true')
    args = parser.parse_args()
    entries = manifest_entries(args.fixtures_manifest)
    if args.inside_container: check_inside(args, entries); return
    if not shutil.which('docker'): raise ValueError('Docker is required for clean Linux runtime checks')
    subprocess.run(['docker','info'], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    download_fixtures(entries, args.fixture_cache)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    scripts = pathlib.Path(__file__).resolve().parent
    install = ('dnf -y -q install python3 procps-ng libcurl-minimal libgomp libstdc++ ca-certificates > /tmp/packages.log'
               if args.runtime.startswith('fedora:') else
               'apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq python3 procps libcurl4 libgomp1 libstdc++6 ca-certificates > /tmp/packages.log')
    command = ['docker','run','--rm','--platform','linux/amd64','--memory=4g','--cpus=2',
               '--mount',f'type=bind,src={args.artifact.resolve()},dst=/input/package{args.artifact.suffix},readonly',
               '--mount',f'type=bind,src={scripts},dst=/probe,readonly',
               '--mount',f'type=bind,src={args.fixtures_manifest.resolve()},dst=/input/fixtures.json,readonly',
               '--mount',f'type=bind,src={args.fixture_cache.resolve()},dst=/fixtures,readonly',
               '--mount',f'type=bind,src={args.output.parent.resolve()},dst=/reports',
               args.runtime,'sh','-ec',
               install + ' && exec "$@"',
               'runtime-check','python3','/probe/check-packaged-runtime.py','--inside-container',
               '--artifact','/input/package'+args.artifact.suffix,'--runtime',args.runtime,
               '--fixtures-manifest','/input/fixtures.json','--fixture-cache','/fixtures',
               '--output','/reports/'+args.output.name]
    subprocess.run(command, check=True)

if __name__ == '__main__':
    try: main()
    except Exception as error:
        print('ERROR: ' + str(error), file=sys.stderr); raise SystemExit(1)
