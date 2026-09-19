"""Read-only, exact-path verification of supplied references. Never executes a DLL."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HANDOFF = ROOT / 'RazKolbas_Codex_Handoff'


def audit_module():
    spec = importlib.util.spec_from_file_location('binary_audit', HANDOFF / 'tools/re/binary_audit.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def inventory(root, baseline):
    root = Path(root).resolve()
    rows = []
    for expected in baseline:
        relative = Path(expected['path'])
        path = (root / relative).resolve()
        if relative.is_absolute() or not path.is_relative_to(root):
            raise ValueError('Reference path escapes root')
        row = dict(expected, absolute_path=str(path), status='MISSING_REFERENCE')
        if path.is_file():
            with path.open('rb') as stream:
                actual = hashlib.file_digest(stream, 'sha256').hexdigest()
            row.update(actual_sha256=actual, actual_size=path.stat().st_size)
            row['status'] = 'VERIFIED' if actual == expected['sha256'] and row['actual_size'] == expected['size'] else 'HASH_MISMATCH'
            # Parse only exact, verified research inputs with the inherited parser.
            if row['status'] == 'VERIFIED' and path.suffix.lower() in ('.dll', '.pdb'):
                audit = audit_module()
                if path.suffix.lower() == '.dll':
                    pe = audit.PE(path)
                    row.update(pe_versions=pe.versions(), codeview=pe.debug())
                else:
                    row['pdb_identity'] = audit.PDB(path).identity()
        rows.append(row)
    return rows


def supplied_baseline():
    evidence = HANDOFF / 'docs/re/evidence'
    rows = []
    for package, manifest in (
        ('SkyrimUpscalerAIOBuild16-Hotfix1', 'skyrim_manifest.json'),
        ('Upscaling Custom 108772 1.65 2026-09-13T17-12Z DzMlEf9Og', 'fallout_manifest.json'),
    ):
        for entry in json.loads((evidence / manifest).read_text()):
            rows.append(dict(entry, path=package + '/' + entry['path']))
    for entry in json.loads((evidence / 'archive_hashes.json').read_text()):
        rows.append({'path': entry['archive'], 'size': entry['size'], 'sha256': entry['sha256']})
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', default=os.environ.get('RAZKOLBAS_REFERENCE_ROOT'))
    parser.add_argument('--verify', action='store_true')
    args = parser.parse_args()
    if not args.root:
        parser.error('Supply --root or RAZKOLBAS_REFERENCE_ROOT; no game directory is assumed')
    rows = inventory(args.root, supplied_baseline())
    print(json.dumps(rows, indent=2))
    return int(args.verify and any(row['status'] != 'VERIFIED' for row in rows))


if __name__ == '__main__':
    raise SystemExit(main())
