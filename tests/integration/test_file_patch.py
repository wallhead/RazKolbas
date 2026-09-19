import hashlib
import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]

class FilePatchTests(unittest.TestCase):
    def setUp(self):
        script = ROOT / 'tools/file_patch.py'
        self.assertTrue(script.is_file(), 'file patch tool unimplemented')
        spec = importlib.util.spec_from_file_location('patch', script)
        self.patch = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.patch)
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.input = self.root / 'input.bin'
        self.output = self.root / 'output.bin'
        self.input.write_bytes(b'\xb8\x01\0\0\0\xc3')
        self.manifest = {'id':'fixture.return-two','purpose':'test fixture', 'kind':'owned-fixture', 'size':6,
            'original_sha256':hashlib.sha256(self.input.read_bytes()).hexdigest(),
            'patched_sha256':hashlib.sha256(b'\xb8\x02\0\0\0\xc3').hexdigest(),
            'patches':[{'offset':1,'expected':'01','replacement':'02'}]}

    def test_dry_run_then_apply_and_restore_preserve_original(self):
        self.patch.patch_file(self.manifest,self.input,self.output,dry_run=True)
        self.assertFalse(self.output.exists())
        self.assertEqual(list(self.root.iterdir()), [self.input])
        self.patch.patch_file(self.manifest,self.input,self.output)
        self.assertEqual(self.input.read_bytes(), b'\xb8\x01\0\0\0\xc3')
        self.assertEqual(self.output.read_bytes(), b'\xb8\x02\0\0\0\xc3')
        restored = self.root / 'restored.bin'
        self.patch.patch_file(self.manifest,self.output,restored,restore=True)
        self.assertEqual(restored.read_bytes(), self.input.read_bytes())

    def test_wrong_hash_wrong_bytes_and_overlap_produce_no_output(self):
        self.manifest['original_sha256'] = '0'*64
        with self.assertRaises(ValueError): self.patch.patch_file(self.manifest,self.input,self.output)
        self.assertFalse(self.output.exists())
        self.manifest['original_sha256'] = hashlib.sha256(self.input.read_bytes()).hexdigest()
        self.manifest['patches'][0]['expected'] = '03'
        with self.assertRaises(ValueError): self.patch.patch_file(self.manifest,self.input,self.output)
        self.assertFalse(self.output.exists())
        self.manifest['patches'][0]['expected'] = '01'
        self.manifest['patches'].append(self.manifest['patches'][0])
        with self.assertRaises(ValueError): self.patch.patch_file(self.manifest,self.input,self.output)
        self.assertFalse(self.output.exists())

    def test_updated_patched_file_and_existing_destination_are_refused(self):
        self.patch.patch_file(self.manifest,self.input,self.output)
        with self.assertRaises(FileExistsError): self.patch.patch_file(self.manifest,self.input,self.output)
        self.output.write_bytes(b'update')
        with self.assertRaises(ValueError): self.patch.patch_file(self.manifest,self.output,self.root/'restore.bin',restore=True)
        self.assertFalse((self.root/'restore.bin').exists())

if __name__ == '__main__': unittest.main()
