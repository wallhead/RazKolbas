"""Pure checks for the bounded, read-only Skyrim UI dimension probe."""

import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools" / "re"))

import inspect_live_detours as probe


class LiveUiDimensionProbeTests(unittest.TestCase):
    def test_exact_ae_dimension_call_reaches_game_callee(self):
        # Decoded exact Skyrim 1.6.1170: ID 106583 + 0x84.
        self.assertTrue(hasattr(probe, "decode_direct_call_target"))
        self.assertEqual(
            probe.decode_direct_call_target(0x14B2E14, bytes.fromhex("e8c7210000")),
            0x14B4FE0,
        )
        self.assertIsNone(
            probe.decode_direct_call_target(0x14B2E1E, bytes.fromhex("0000488b88"))
        )

    def test_two_adjacent_game_dimension_pairs_stay_distinct(self):
        state = bytearray(0x120)
        struct.pack_into("<IIII", state, 0x24, 2560, 1440, 1707, 960)
        self.assertTrue(hasattr(probe, "dimension_pairs"))
        self.assertEqual(probe.dimension_pairs(state), ((2560, 1440), (1707, 960)))


if __name__ == "__main__":
    unittest.main()
