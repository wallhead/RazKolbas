"""Find hash-independent decoded xrefs to a known Skyrim static RVA.

This is offline triage of a captured .text section. Each reported LEA must
still be verified in its surrounding function before use as a hook contract.
"""
import argparse
import json
import struct
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--text", type=Path, required=True)
    parser.add_argument("--base-rva", type=lambda value: int(value, 0), default=0x1000)
    parser.add_argument("--target-rva", type=lambda value: int(value, 0), required=True)
    args = parser.parse_args()
    code = args.text.read_bytes()
    matches = []
    for prefix in (b"\x48\x8d", b"\x4c\x8d"):
        position = code.find(prefix)
        while position >= 0 and position + 7 <= len(code):
            if code[position + 2] & 0xc7 == 0x05:
                displacement = struct.unpack_from("<i", code, position + 3)[0]
                if args.base_rva + position + 7 + displacement == args.target_rva:
                    matches.append({"rva": hex(args.base_rva + position),
                                    "bytes": code[position:position + 7].hex()})
            position = code.find(prefix, position + 1)
    print(json.dumps(sorted(matches, key=lambda item: int(item["rva"], 16))))


if __name__ == "__main__":
    main()
