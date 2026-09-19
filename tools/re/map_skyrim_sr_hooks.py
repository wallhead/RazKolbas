"""Hash-verify Skyrim 1.6.1170 Address Library and map reference SR hook IDs.

Read-only decoder follows pinned CommonLibSSE-NG IDDatabase format 2. Reference
hook IDs/addends were recovered from hash-gated SkyrimUpscaler.dll install code.
Neither the game nor reference binary is executed or patched by this script.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

EXPECTED = 'c4093c569a3c83b26587f4b9ea4c55de9ae6e73b84a2af9fb3fbd30e2fe0d452'
TARGETS = {
    77245: {'reference': 'BSGraphics_Renderer_Begin_UpdateJitter reference hook sites', 'addends': [0xe5, 0x133]},
    77247: {'reference': 'Live world-draw CALL target observed on 2026-09-20; engine routine semantics still unverified', 'addends': []},
    77518: {'reference': 'updateJitterHook relocation operand; patched site requires further tracing', 'addends': []},
    77520: {'reference': 'buildCameraStateDataHook relocation operand; patched site requires further tracing', 'addends': []},
    82084: {'reference': 'Main_DrawWorld_MainDraw call-site hook; pre-UI semantics unverified', 'addends': [0x17a]},
}


def decode(path):
    raw = path.read_bytes()
    if hashlib.sha256(raw).hexdigest() != EXPECTED:
        raise ValueError('Address Library hash mismatch')
    at = 0
    def take(code):
        nonlocal at
        size = struct.calcsize(code)
        if at + size > len(raw):
            raise ValueError('Truncated Address Library')
        result = struct.unpack_from(code, raw, at)[0]
        at += size
        return result
    if take('<i') != 2:
        raise ValueError('Expected format 2')
    version = [take('<i') for _ in range(4)]
    if version != [1, 6, 1170, 0]:
        raise ValueError('Version mismatch')
    name_length = take('<i')
    if not 0 <= name_length <= 4096:
        raise ValueError('Invalid name length')
    at += name_length
    pointer_size, count = take('<i'), take('<i')
    if pointer_size != 8 or count != 428461:
        raise ValueError('Header mismatch')
    prev_id = prev_offset = 0
    found = {}
    for _ in range(count):
        kind = take('<B')
        lo, hi = kind & 0xf, kind >> 4
        if lo == 0: ident = take('<Q')
        elif lo == 1: ident = prev_id + 1
        elif lo == 2: ident = prev_id + take('<B')
        elif lo == 3: ident = prev_id - take('<B')
        elif lo == 4: ident = prev_id + take('<H')
        elif lo == 5: ident = prev_id - take('<H')
        elif lo == 6: ident = take('<H')
        elif lo == 7: ident = take('<I')
        else: raise ValueError('Invalid ID encoding')
        basis = prev_offset // pointer_size if hi & 8 else prev_offset
        mode = hi & 7
        if mode == 0: offset = take('<Q')
        elif mode == 1: offset = basis + 1
        elif mode == 2: offset = basis + take('<B')
        elif mode == 3: offset = basis - take('<B')
        elif mode == 4: offset = basis + take('<H')
        elif mode == 5: offset = basis - take('<H')
        elif mode == 6: offset = take('<H')
        elif mode == 7: offset = take('<I')
        else: raise ValueError('Invalid offset encoding')
        if hi & 8: offset *= pointer_size
        if ident in TARGETS:
            if ident in found: raise ValueError('Duplicate target ID')
            found[ident] = offset
        prev_id, prev_offset = ident, offset
    if at != len(raw) or set(found) != set(TARGETS):
        raise ValueError('Incomplete or trailing Address Library data')
    return {'version': version, 'sha256': EXPECTED, 'records': count,
            'targets': [{'id': ident, 'base_rva': hex(found[ident]),
                         'reference': spec['reference'],
                         'site_rvas': [hex(found[ident] + x) for x in spec['addends']]}
                        for ident, spec in TARGETS.items()]}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--library', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    result = decode(a.library)
    a.output.parent.mkdir(parents=True, exist_ok=True)
    a.output.write_text(json.dumps(result, indent=2))
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
