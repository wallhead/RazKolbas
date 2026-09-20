"""Validate a capture, run the isolated NVIDIA DLAA replay, and inspect output.

Requires NumPy/Pillow. This measures reset-frame output, not live temporal
correctness, source-stage placement, or performance. Capture inputs stay local.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import os
import numpy as np
from PIL import Image, ImageDraw

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--exe', type=Path, required=True)
p.add_argument('--runtime-directory', type=Path, required=True)
p.add_argument('--capture', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
depth_mode = os.environ.get('RAZKOLBAS_SR_REPLAY_DEPTH_MODE', 'normalized')
if depth_mode not in ('normalized', 'typeless'):
    raise ValueError('Unknown replay depth mode')
output = a.output/'dlss-output-rgba16f.raw'
if output.exists():
    raise ValueError('Use a fresh output directory; never validate stale GPU output')
verified = json.loads(subprocess.check_output([sys.executable,
    str(Path(__file__).with_name('inspect_candidate_capture.py')), str(a.capture)], text=True))
for label, fmt, channels, row in [('main-colour-candidate', 10, 4, 20480), ('motion-candidate', 34, 2, 10240), ('depth-candidate', 44, 0, 10240)]:
    meta = verified[label]
    if [int(meta[k]) for k in ('width', 'height', 'format', 'rowBytes')] != [2560, 1440, fmt, row]:
        raise ValueError('Replay requires tightly packed 2560x1440 formats10/34/44')
    if channels and any(c['nonfinite'] for c in meta['channels']):
        raise ValueError('Replay input contains nonfinite values')
a.output.mkdir(parents=True, exist_ok=True)
(a.output/'verified-inputs.json').write_text(json.dumps(verified, indent=2))
command = [str(a.exe.resolve()), str(a.runtime_directory.resolve()), str(a.capture.resolve()), str(a.output.resolve())]
try:
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)
except subprocess.TimeoutExpired as error:
    (a.output/'replay.log').write_bytes(error.stdout or b'')
    raise
(a.output/'replay.log').write_bytes(result.stdout)
print(result.stdout.decode(errors='replace'))
if result.returncode or b'SINGLE_FRAME_DLAA_REPLAY=PASS' not in result.stdout:
    raise RuntimeError(f'Replay failed: process exit {result.returncode}')
if output.stat().st_size != 2560*1440*8:
    raise ValueError('Output extent differs')
source = np.fromfile(a.capture/'main-colour-candidate.raw', '<f2').reshape(1440, 2560, 4).astype(np.float32)
pixels = np.fromfile(output, '<f2').reshape(1440, 2560, 4).astype(np.float32)
rgb = pixels[:, :, :3]
if not np.isfinite(rgb).all() or float(np.ptp(rgb)) <= 0 or np.array_equal(source[:, :, :3], rgb):
    raise ValueError('Output nonfinite, uniform or identical to input')
report = {'scope': 'offline single reset frame DLAA; no temporal-quality claim',
    'output_sha256': hashlib.sha256(output.read_bytes()).hexdigest(), 'shape': list(pixels.shape),
    'rgb_finite': True, 'rgb_minmax': [float(rgb.min()), float(rgb.max())],
    'alpha_finite': bool(np.isfinite(pixels[:, :, 3]).all()),
    'rgb_mean_absolute_change': float(np.abs(source[:, :, :3]-rgb).mean()),
    'changed_rgb_fraction': float(np.mean(source[:, :, :3] != rgb)),
    'experiment': {'reset': True, 'jitter': [0, 0], 'mv_scale': [2560, 1440],
                   'depth': ('native R24G8_TYPELESS, no conversion' if depth_mode == 'typeless'
                             else 'low24 / 16777215 -> R32_FLOAT; not linearized'),
                   'flags': ['HDR', 'MVLowRes', 'AutoExposure']}}
(a.output/'validation.json').write_text(json.dumps(report, indent=2, allow_nan=False))
canvas = Image.new('RGB', (1600, 500), '#15181c')
draw = ImageDraw.Draw(canvas)
for i, (values, label) in enumerate([(source, 'Captured Skyrim colour'), (pixels, 'Supplied DLSS runtime: reset-frame DLAA')]):
    preview = np.round(np.clip(values[:, :, :3], 0, 1)**(1/2.2)*255).astype(np.uint8)
    canvas.paste(Image.fromarray(preview).resize((800, 450)), (i*800, 40))
    draw.text((i*800+12, 14), label, fill='white')
canvas.save(a.output/'preview.png')
print(json.dumps(report, indent=2))
