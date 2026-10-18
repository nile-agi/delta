#!/usr/bin/env python3
"""Validate package runtime gate failures without downloading weights or running Docker."""
import hashlib
import importlib.util
import json
import pathlib
import struct
import tempfile
import unittest
from unittest import mock
from types import SimpleNamespace

spec = importlib.util.spec_from_file_location('packaged_runtime', pathlib.Path(__file__).with_name('check-packaged-runtime.py'))
gate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gate)

class PackageGateTests(unittest.TestCase):
    def test_dependency_failure_keeps_a_diagnostic_report(self):
        with tempfile.TemporaryDirectory() as folder:
            root = pathlib.Path(folder)
            args = SimpleNamespace(output=root/'report.json', artifact=root/'broken.deb', runtime='ubuntu:24.04')
            with mock.patch.object(gate, 'find_engines', return_value=(root/'delta-server', root/'llama-server')), \
                 mock.patch.object(gate.subprocess, 'run', return_value=SimpleNamespace(
                     stdout='libmissing.so => not found', stderr='', returncode=0)):
                with self.assertRaisesRegex(ValueError, 'Unresolved packaged dependency'):
                    gate.check_inside(args, [])
            report = json.loads(args.output.read_text())
            self.assertIn('not found', report['error'])
            self.assertIn('not found', report['dependencies']['delta-server'])

    def test_clean_container_installs_process_monitor_and_matches_release_architecture(self):
        with tempfile.TemporaryDirectory() as folder:
            root = pathlib.Path(folder)
            for runtime, process_package in [('fedora:44', 'procps-ng'), ('ubuntu:24.04', 'procps'),
                                             ('linuxmintd/mint22.3-amd64:latest', 'procps')]:
                with self.subTest(runtime=runtime), \
                     mock.patch('sys.argv', ['check-packaged-runtime.py', '--artifact', str(root/'package.deb'),
                         '--runtime', runtime, '--fixtures-manifest', str(root/'fixtures.json'),
                         '--fixture-cache', str(root/'cache'), '--output', str(root/'result.json')]), \
                     mock.patch.object(gate, 'manifest_entries', return_value=[]), \
                     mock.patch.object(gate, 'download_fixtures'), \
                     mock.patch.object(gate.shutil, 'which', return_value='/usr/bin/docker'), \
                     mock.patch.object(gate.subprocess, 'run') as run:
                    gate.main()
                    command = run.call_args.args[0]
                    self.assertIn(process_package, command[command.index('-ec') + 1])
                    self.assertEqual(command[command.index('--platform') + 1], 'linux/amd64')

    def test_missing_or_ambiguous_package_engines_fail(self):
        with tempfile.TemporaryDirectory() as folder:
            root = pathlib.Path(folder)
            with self.assertRaises(ValueError): gate.find_engines(root)
            for name in ['delta-server', 'llama-server']:
                (root / name).write_bytes(b'fake')
            self.assertEqual(gate.find_engines(root)[0].name, 'delta-server')
            (root / 'other').mkdir()
            (root / 'other/delta-server').write_bytes(b'fake')
            with self.assertRaises(ValueError): gate.find_engines(root)

    def test_build_tree_libraries_cannot_mask_broken_package(self):
        with tempfile.TemporaryDirectory() as folder:
            with self.assertRaises(ValueError): gate.check_library_paths(pathlib.Path(folder), {'LD_LIBRARY_PATH':'/build/lib'})
            gate.check_library_paths(pathlib.Path(folder), {})

    def test_fixture_hash_size_and_quantization_are_checked(self):
        with tempfile.TemporaryDirectory() as folder:
            path = pathlib.Path(folder) / 'fixture.gguf'
            path.write_bytes(b'GGUF' + struct.pack('<IQQ',3,1,0) + struct.pack('<Q',1) + b'x' + struct.pack('<IQIQ',1,32,0,0) + bytes(128))
            entry = {'bytes':path.stat().st_size,'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'tensor_types':[12]}
            with self.assertRaisesRegex(ValueError,'quantized'):gate.validate_fixture(path,entry)
            entry['tensor_types']=[0];entry['sha256']='0'*64
            with self.assertRaisesRegex(ValueError,'checksum'):gate.validate_fixture(path,entry)
            entry['bytes']+=1
            with self.assertRaisesRegex(ValueError,'size'):gate.validate_fixture(path,entry)

    def test_fixture_budget_and_pinned_source_are_required(self):
        with tempfile.TemporaryDirectory() as folder:
            path = pathlib.Path(folder) / 'manifest.json'
            entry = {'model':'gemma3:270m','filename':'fixture.gguf','url':'https://huggingface.co/org/repo/resolve/'+'a'*40+'/fixture.gguf',
                     'bytes':900*1024*1024,'sha256':'a'*64,'tensor_types':[8]}
            path.write_text(json.dumps([entry]))
            with self.assertRaisesRegex(ValueError,'budget'):gate.manifest_entries(path)
            entry.update(bytes=1024,url='https://huggingface.co/org/repo/resolve/main/fixture.gguf')
            path.write_text(json.dumps([entry]))
            with self.assertRaisesRegex(ValueError,'pinned'):gate.manifest_entries(path)

    def test_failed_model_probe_rejects_artifact(self):
        with self.assertRaises(ValueError):gate.verify_probe_result({'checks':{'generated_text':False}})
        with self.assertRaises(ValueError):gate.verify_probe_result({'error':'worker died','checks':{'generated_text':True}})

if __name__ == '__main__':unittest.main()
