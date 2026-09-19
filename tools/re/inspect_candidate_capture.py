"""Verify a bounded raw candidate bundle and report numerical ranges, not guide semantics.

Requires NumPy. Files stay local; input is the directory emitted by RazKolbas.
"""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np

parser = argparse.ArgumentParser()
parser.add_argument("directory", type=Path)
args = parser.parse_args()
lines = (args.directory / "manifest.txt").read_text().splitlines()
if not lines or lines[-1] != "complete=true":
    raise ValueError("Incomplete capture bundle")
labels = {"main-colour-candidate", "motion-candidate", "depth-candidate"}
report = {}
for line in lines:
    words = line.split()
    if not words or words[0] not in labels:
        continue
    label = words[0]
    if label in report:
        raise ValueError("Duplicate candidate")
    fields = dict(word.split("=", 1) for word in words[1:])
    expected = int(fields["bytes"])
    if not 0 < expected <= 64 * 1024 * 1024:
        raise ValueError("Candidate byte extent exceeds limit")
    raw_path = args.directory / (label + ".raw")
    if raw_path.stat().st_size != expected:
        raise ValueError("Candidate file extent differs")
    raw = raw_path.read_bytes()
    if hashlib.sha256(raw).hexdigest() != fields["sha256"]:
        raise ValueError("Candidate hash differs")
    width, height = int(fields["width"]), int(fields["height"])
    row = int(fields["rowBytes"])
    if width <= 0 or height <= 0 or row * height != len(raw):
        raise ValueError("Candidate row layout differs")
    item = {**fields, "verified": True, "nonzero_bytes": int(np.count_nonzero(np.frombuffer(raw, dtype=np.uint8)))}
    fmt = int(fields["format"])
    # Numerical interpretation only, with no assertion of semantic validity.
    spec = {10: ("<f2", 4), 34: ("<f2", 2), 41: ("<f4", 1), 39: ("<f4", 1), 28: ("u1", 4)}.get(fmt)
    if spec:
        dtype, channels = spec
        values = np.frombuffer(raw, dtype=dtype).reshape(height, width, channels)
        item["channels"] = []
        for channel in range(channels):
            data = values[:, :, channel]
            finite = data[np.isfinite(data)]
            item["channels"].append({"finite": int(finite.size), "nonfinite": int(data.size - finite.size),
                                     "min": float(finite.min()) if finite.size else None,
                                     "max": float(finite.max()) if finite.size else None})
    report[label] = item
if set(report) != labels:
    raise ValueError("Missing candidates")
print(json.dumps(report, indent=2, allow_nan=False))
