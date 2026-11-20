#!/usr/bin/env python3
"""shots.py <dir> <out.png> [cols] [scale]: contact sheet of every frame*.bmp in <dir>; then deletes the BMPs."""
import sys, glob, os
from PIL import Image
d, out = sys.argv[1], sys.argv[2]
cols = int(sys.argv[3]) if len(sys.argv) > 3 else 3
scale = float(sys.argv[4]) if len(sys.argv) > 4 else 1.0
fs = sorted(glob.glob(os.path.join(d, 'frame*.bmp')))
ims = [Image.open(f).convert('RGB') for f in fs]
if scale != 1.0: ims = [im.resize((int(im.width * scale), int(im.height * scale))) for im in ims]
rows = (len(ims) + cols - 1) // cols
w, h = ims[0].size
sheet = Image.new('RGB', (w * cols, h * rows))
for i, im in enumerate(ims): sheet.paste(im, (w * (i % cols), h * (i // cols)))
sheet.save(out)
for f in fs: os.remove(f)
print(len(fs), 'frames ->', out)
