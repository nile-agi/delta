import importlib.util
import pathlib
import struct
import unittest

spec = importlib.util.spec_from_file_location('verify', pathlib.Path(__file__).with_name('verify-release-binaries.py'))
verify = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verify)

class ReleaseBinaryTests(unittest.TestCase):
    def macho(self, minimum):
        return struct.pack('<8I', 0xfeedfacf, 0x1000007, 3, 2, 1, 24, 0, 0) + struct.pack('<6I', 0x32, 24, 1, minimum << 16, 26 << 16, 0)

    def test_rejects_published_backend_that_requires_macos_26(self):
        with self.assertRaisesRegex(ValueError, '26.0.0'):
            verify.inspect_binary(self.macho(26), 'x86_64-apple-darwin')

    def test_accepts_backend_matching_declared_macos_minimum(self):
        verify.inspect_binary(self.macho(11), 'x86_64-apple-darwin')

    def test_rejects_wrong_architecture_and_missing_load_commands(self):
        with self.assertRaises(ValueError):
            verify.inspect_binary(self.macho(11), 'aarch64-apple-darwin')
        with self.assertRaises(ValueError):
            verify.inspect_binary(self.macho(11)[:32], 'x86_64-apple-darwin')

if __name__ == '__main__':
    unittest.main()
