import ctypes
import sys
import unittest
from pathlib import Path

DLL = Path(sys.argv.pop(1)).resolve()

class PluginTests(unittest.TestCase):
    def test_real_dll_has_skse_exports_and_rejects_null_loader(self):
        self.assertTrue(DLL.is_file(), 'RazKolbas.dll missing')
        library = ctypes.WinDLL(str(DLL))
        query = library.SKSEPlugin_Query
        query.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        query.restype = ctypes.c_bool
        load = library.SKSEPlugin_Load
        load.argtypes = [ctypes.c_void_p]
        load.restype = ctypes.c_bool
        self.assertFalse(query(None, None))
        self.assertFalse(load(None))
        self.assertEqual(ctypes.c_uint32.in_dll(library, 'SKSEPlugin_Version').value, 1)

if __name__ == '__main__':
    unittest.main()
