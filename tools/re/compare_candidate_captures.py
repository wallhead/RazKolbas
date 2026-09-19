"""Compare verified raw candidate bundles; no guide semantics are assumed.

NumPy/Pillow required. Labels describe the requested test order, not inferred
camera state. Produces numerical JSON and a diagnostic raster contact sheet.
"""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import numpy as np
from PIL import Image, ImageDraw

p = argparse.ArgumentParser()
p.add_argument('--capture', action='append', required=True, help='label=directory')
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
if not 1 <= len(a.capture) <= 6:
    raise ValueError('One to six captures required')
records, arrays = [], []
for entry in a.capture:
    label, name = entry.split('=', 1)
    directory = Path(name)
    verified = json.loads(subprocess.check_output([sys.executable, str(Path(__file__).with_name('inspect_candidate_capture.py')), str(directory)], text=True))
    colour_meta, motion_meta, depth_meta = [verified[k] for k in ('main-colour-candidate', 'motion-candidate', 'depth-candidate')]
    if [int(m['format']) for m in (colour_meta, motion_meta, depth_meta)] != [10, 34, 44]:
        raise ValueError('This comparison supports formats10/34/44 only')
    w, h = int(colour_meta['width']), int(colour_meta['height'])
    if any(int(m['width']) != w or int(m['height']) != h for m in (motion_meta, depth_meta)):
        raise ValueError('Candidate dimensions differ')
    colour = np.fromfile(directory/'main-colour-candidate.raw', '<f2').reshape(h, w, 4).astype(np.float32)
    motion = np.fromfile(directory/'motion-candidate.raw', '<f2').reshape(h, w, 2).astype(np.float32)
    raw_depth = np.fromfile(directory/'depth-candidate.raw', '<u4').reshape(h, w)
    depth = raw_depth & 0xffffff
    if not np.isfinite(colour).all() or not np.isfinite(motion).all():
        raise ValueError('Nonfinite candidate values')
    magnitude = np.linalg.norm(motion, axis=2)
    def motion_stats(mask):
        v, m = motion[mask], magnitude[mask]
        if not m.size:
            return None
        return {'pixels': int(m.size), 'median_xy': np.median(v, axis=0).tolist(),
                'magnitude_p50_p95_p99_max': np.percentile(m, [50, 95, 99, 100]).tolist(),
                'rms': float(np.sqrt(np.mean(m.astype(np.float64)**2))), 'nonzero_fraction': float(np.mean(m != 0))}
    rec = {'label': label, 'directory': str(directory), 'verified_files': verified,
           'motion_all': motion_stats(np.ones((h, w), bool)), 'motion_nonclear_depth': motion_stats(depth != 0xffffff),
           'depth_low24': {'min': int(depth.min()), 'max': int(depth.max()), 'distinct': int(np.unique(depth).size),
                           'clear_max_fraction': float(np.mean(depth == 0xffffff))},
           'depth_high8_values': np.unique(raw_depth >> 24).tolist()}
    records.append(rec)
    # Preview arithmetic is explicit; no colour encoding/depth linearization is inferred.
    rgb = np.clip(colour[:, :, :3], 0, 1)**(1/2.2)
    depth_preview = (np.log10(np.maximum(1-depth.astype(np.float32)/0xffffff, 1/0xffffff))+7.3)/7.3
    arrays.append((rgb, np.clip(depth_preview, 0, 1), magnitude))
a.output.mkdir(parents=True, exist_ok=True)
scale = max(float(np.percentile(frame[2], 99)) for frame in arrays) or 1
panel_w = 640
panel_h = round(panel_w*arrays[0][0].shape[0]/arrays[0][0].shape[1])
canvas = Image.new('RGB', (panel_w*3, (panel_h+42)*len(arrays)), '#15181c')
draw = ImageDraw.Draw(canvas)
for i, (rec, (rgb, depth_preview, magnitude)) in enumerate(zip(records, arrays)):
    heat = np.clip(magnitude/scale, 0, 1)
    previews = (rgb, np.repeat(depth_preview[:, :, None], 3, axis=2), np.stack((heat, heat**2, np.zeros_like(heat)), axis=2))
    titles = (rec['label']+' | clipped RGB, display gamma 2.2', 'log10(1 - low24/0xffffff), range -7.3..0', f'raw MV magnitude | shared 0..{scale:.6g}')
    for col, (data, title) in enumerate(zip(previews, titles)):
        x, y = col*panel_w, i*(panel_h+42)
        draw.text((x+10, y+12), title, fill='white')
        img = Image.fromarray(np.round(np.clip(data, 0, 1)*255).astype(np.uint8)).resize((panel_w, panel_h))
        canvas.paste(img, (x, y+42))
canvas.save(a.output/'comparison.png')
(a.output/'comparison.json').write_text(json.dumps(records, indent=2, allow_nan=False))
print(json.dumps([{k: v for k, v in r.items() if k != 'verified_files'} for r in records], indent=2))
