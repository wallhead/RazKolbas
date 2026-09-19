"""Fetch the pinned official NVIDIA SDK to a local, untracked directory.

Does not fetch or replace a DLSS runtime. Verifies Git blob identities against
the selected immutable commit and records SHA256 for each downloaded file.
"""
import argparse
import hashlib
import json
from pathlib import Path
import urllib.request

COMMIT = '374959484e79a640feaba44c93ac8cfb0a03f5b5'
LIBRARIES = {'lib/Windows_x86_64/x64/nvsdk_ngx_d.lib', 'lib/Windows_x86_64/x64/nvsdk_ngx_d_dbg.lib'}
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
tree = json.load(urllib.request.urlopen(f'https://api.github.com/repos/NVIDIA/DLSS/git/trees/{COMMIT}?recursive=1', timeout=30))
records = []
for item in tree['tree']:
    name = item['path']
    if item['type'] != 'blob' or not (name.startswith('include/') or name in LIBRARIES):
        continue
    # Only the flat header directory and two explicit libraries are selected.
    if name not in LIBRARIES and ('/' in name[len('include/'):] or not name.endswith('.h')):
        continue
    data = urllib.request.urlopen(f'https://raw.githubusercontent.com/NVIDIA/DLSS/{COMMIT}/{name}', timeout=30).read()
    blob = hashlib.sha1(f'blob {len(data)}\0'.encode()+data).hexdigest()
    if blob != item['sha']:
        raise ValueError(f'Git blob identity mismatch: {name}')
    target = a.output/name
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(data)
    records.append({'path': name, 'sha256': hashlib.sha256(data).hexdigest(), 'git_blob': blob})
(a.output/'provenance.json').write_text(json.dumps({'commit': COMMIT, 'files': records}, indent=2))
print(f'Fetched and verified {len(records)} pinned SDK files; no runtime downloaded')
