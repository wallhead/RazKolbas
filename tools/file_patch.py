"""Exact-hash working-copy file patches. In-place replacement is deliberately unsupported."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import tempfile

def digest(data):
    return hashlib.sha256(data).hexdigest()

def validate_pe(data):
    if len(data) < 64 or data[:2] != b'MZ': raise ValueError('Invalid DOS header')
    pe = struct.unpack_from('<I', data, 60)[0]
    if pe + 264 > len(data) or data[pe:pe+4] != b'PE\0\0': raise ValueError('Invalid PE header')
    if struct.unpack_from('<H', data, pe+4)[0] != 0x8664 or struct.unpack_from('<H', data, pe+24)[0] != 0x20b:
        raise ValueError('Expected Windows x64 PE32+')
    sections = struct.unpack_from('<H', data, pe+6)[0]
    start = pe + 24 + struct.unpack_from('<H', data, pe+20)[0]
    if start + 40*sections > len(data): raise ValueError('Truncated section table')
    for at in range(start, start + sections*40, 40):
        size, offset = struct.unpack_from('<II', data, at+16)
        if offset + size > len(data): raise ValueError('Section outside file')

def write_new(path, data):
    """Publish a flushed complete file without ever replacing another file."""
    descriptor, temporary = tempfile.mkstemp(prefix='.rk-', dir=path.parent)
    try:
        with os.fdopen(descriptor, 'wb') as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.link(temporary, path)  # Atomic exclusive publication on the same volume.
    finally:
        os.unlink(temporary)

def patch_file(manifest, source, output, dry_run=False, restore=False):
    source, output = Path(source).resolve(), Path(output).resolve()
    if source == output: raise ValueError('Use a separate working-copy output')
    if output.exists(): raise FileExistsError(output)
    if not manifest.get('id') or not manifest.get('purpose'): raise ValueError('Patch identity/purpose required')
    if manifest.get('kind') not in ('owned-fixture', 'pe-x64'): raise ValueError('Declare owned-fixture or pe-x64')
    original = source.read_bytes()
    expected_hash = manifest['patched_sha256' if restore else 'original_sha256']
    result_hash = manifest['original_sha256' if restore else 'patched_sha256']
    if len(original) != manifest['size'] or digest(original) != expected_hash: raise ValueError('HASH_MISMATCH')
    if manifest['kind'] == 'pe-x64': validate_pe(original)
    changed = bytearray(original)
    occupied = set()
    if not manifest.get('patches'): raise ValueError('Empty patch set')
    for patch in manifest['patches']:
        offset = patch['offset']
        expected = bytes.fromhex(patch['replacement' if restore else 'expected'])
        replacement = bytes.fromhex(patch['expected' if restore else 'replacement'])
        if not isinstance(offset,int) or offset < 0 or not expected or len(expected) != len(replacement) or offset + len(expected) > len(original):
            raise ValueError('Invalid patch range')
        extent = set(range(offset, offset + len(expected)))
        if occupied & extent: raise ValueError('Overlapping patches')
        occupied.update(extent)
        if original[offset:offset+len(expected)] != expected: raise ValueError('EXPECTED_BYTES_MISMATCH')
        changed[offset:offset+len(expected)] = replacement
    if digest(changed) != result_hash: raise ValueError('Patched hash differs from manifest')
    if manifest['kind'] == 'pe-x64': validate_pe(changed)
    journal = {'id':manifest['id'], 'input_sha256':digest(original), 'output_sha256':digest(changed), 'restore':restore, 'size':len(changed)}
    if dry_run: return dict(journal, status='DRY_RUN')
    # Sidecars are created before publication so a visible output has its backup.
    # A failure can leave sidecars for inspection; it cannot damage input/output.
    backup = Path(str(output) + '.original')
    journal_path = Path(str(output) + '.patch.json')
    if backup.exists() or journal_path.exists(): raise FileExistsError('Existing patch sidecar')
    write_new(backup, original)
    write_new(journal_path, json.dumps(journal, indent=2).encode('utf-8'))
    write_new(output, changed)
    if digest(output.read_bytes()) != result_hash: raise OSError('Output readback failed')
    return dict(journal, status='RESTORED' if restore else 'PATCHED')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--what-if', action='store_true')
    parser.add_argument('--restore', action='store_true')
    args = parser.parse_args()
    print(json.dumps(patch_file(json.loads(args.manifest.read_text(encoding='utf-8-sig')), args.input, args.output, args.what_if, args.restore), indent=2))

if __name__ == '__main__': main()
