#!/usr/bin/env python3
"""Bakes the background gallery: bake_backgrounds.py <concepts-dir> <out-dir>
Each picture is cropped to 4:3, scaled to 640x480 and written as RGB565 with ordered dithering:
  <out-dir>/NN-Name.bg   = "MPBG", u16 width, u16 height, then width*height little-endian RGB565.
Pictures that show third-party artwork are deliberately left out (see SKIP)."""
import os, struct, sys
import numpy as np
from PIL import Image

# (source file, display name, where to crop the 4:3 band: 0 = keep the top, 1 = keep the bottom)
GALLERY = [   # six on purpose: a small, varied set (the full set of concepts stays in Extras/Concepts)
    ('Pixel Minesweeper Sunset Valley.png',           'Sunset Valley',          0.55),
    ('Pixel Art Crystal Mine Cave.png',               'Crystal Cave',           0.50),
    ('Moonlit Minesweeper Battlefield.png',           'Moonlit Battlefield',    0.55),
    ('Desert Minefield at Sunset.png',                'Desert Sunset',          0.55),
    ('Frozen Battlefield Beneath the Aurora.png',     'Frozen Aurora',          0.45),
    ('Retro Minesweeper Mountain Landscape.png',      'Retro Mountains',        0.60),
]
SKIP = {   # shown Fallout (Bethesda) emblems / Pip-Boy / vault suit
    'Nuclear Winter Wasteland with Frosted Mines.png',
    'Pixel-Art Nuclear Winter Wasteland.png',
}
BAYER = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]], dtype=np.float32) / 16.0 - 0.5

def to565(im):
    a = np.asarray(im.convert('RGB'), dtype=np.float32)
    h, w, _ = a.shape
    d = np.tile(BAYER, (h // 4 + 1, w // 4 + 1))[:h, :w, None]
    r = np.clip(np.round(a[..., 0] / 255 * 31 + d[..., 0]), 0, 31).astype(np.uint16)
    g = np.clip(np.round(a[..., 1] / 255 * 63 + d[..., 0]), 0, 63).astype(np.uint16)
    b = np.clip(np.round(a[..., 2] / 255 * 31 + d[..., 0]), 0, 31).astype(np.uint16)
    return ((r << 11) | (g << 5) | b).astype('<u2').tobytes()

src, out = sys.argv[1], sys.argv[2]
os.makedirs(out, exist_ok=True)
for f in os.listdir(out):
    if f.endswith('.bg'): os.remove(os.path.join(out, f))
for i, (fn, name, pos) in enumerate(GALLERY, 1):
    im = Image.open(os.path.join(src, fn)).convert('RGB')
    w, h = im.size
    ch = int(w * 3 / 4)                       # height of the 4:3 band at full width
    top = int((h - ch) * pos)
    im = im.crop((0, top, w, top + ch)).resize((640, 480), Image.LANCZOS)
    path = os.path.join(out, '%02d-%s.bg' % (i, name.replace(' ', '_')))
    with open(path, 'wb') as fh:
        fh.write(b'MPBG' + struct.pack('<HH', 640, 480) + to565(im))
    print(path)

