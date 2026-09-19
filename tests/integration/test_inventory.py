import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('inventory', ROOT / 'tools/reference_inventory.py')


class InventoryTests(unittest.TestCase):
    def setUp(self):
        self.assertTrue(Path(SPEC.origin).exists(), 'reference inventory implementation missing')
        self.module = importlib.util.module_from_spec(SPEC)
        SPEC.loader.exec_module(self.module)
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.baseline = [{'path': 'reference.bin', 'sha256': hashlib.sha256(b'original').hexdigest(), 'size': 8}]
        self.unrelated = self.root / 'my-uncommitted-work.txt'
        self.unrelated.write_bytes(b'do not change')

    def test_missing_reference_is_reported_without_writes(self):
        result = self.module.inventory(self.root, self.baseline)
        self.assertEqual(result[0]['status'], 'MISSING_REFERENCE')
        self.assertEqual(list(self.root.iterdir()), [self.unrelated])

    def test_changed_byte_does_not_replace_reference_or_baseline(self):
        target = self.root / 'reference.bin'
        target.write_bytes(b'originaL')
        before = json.dumps(self.baseline)
        result = self.module.inventory(self.root, self.baseline)
        self.assertEqual(result[0]['status'], 'HASH_MISMATCH')
        self.assertEqual(target.read_bytes(), b'originaL')
        self.assertEqual(json.dumps(self.baseline), before)
        self.assertEqual(self.unrelated.read_bytes(), b'do not change')

    def test_valid_reference_retains_unrelated_changes(self):
        (self.root / 'reference.bin').write_bytes(b'original')
        result = self.module.inventory(self.root, self.baseline)
        self.assertEqual(result[0]['status'], 'VERIFIED')
        self.assertEqual(self.unrelated.read_bytes(), b'do not change')

    def test_manifest_cannot_escape_reference_root(self):
        for path in ('../escape.bin', str(self.root.parent / 'escape.bin')):
            with self.assertRaises(ValueError):
                self.module.inventory(self.root, [{'path': path, 'sha256': '0'*64, 'size': 1}])


if __name__ == '__main__':
    unittest.main()
