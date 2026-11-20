#!/usr/bin/env python3
"""Makes package-src/boxart.png (500x500) from a screenshot of the game: make_boxart.py <frame.bmp> <out.png>
(the screenshot comes from the host build: MP_SEED=7 + the pad script in DEV-NOTES.md)."""
import sys
from PIL import Image, ImageDraw, ImageFont
src = Image.open(sys.argv[1]).convert('RGB')
bg = src.getpixel((2, 2))
W, H = src.size
rows = [y for y in range(0, H - 40) if any(src.getpixel((x, y)) != bg for x in range(0, W, 3))]
cols = [x for x in range(W) if any(src.getpixel((x, y)) != bg for y in range(rows[0], rows[-1], 3))]
board = src.crop((cols[0], rows[0], cols[-1] + 1, rows[-1] + 1))
k = min(430 / board.width, 330 / board.height)
board = board.resize((int(board.width * k), int(board.height * k)), Image.LANCZOS)
im = Image.new('RGB', (500, 500))
d = ImageDraw.Draw(im)
for y in range(500):                       # teal vertical gradient, like the original desktop
    k = y / 499
    d.line([(0, y), (500, y)], fill=(int(0 + 10 * k), int(96 - 40 * k), int(96 - 40 * k)))
f1 = ImageFont.truetype('/usr/share/fonts/dejavu/DejaVuSans-Bold.ttf', 50)
f2 = ImageFont.truetype('/usr/share/fonts/dejavu/DejaVuSans.ttf', 24)
def centred(text, y, font, fill, shadow=True):
    w = d.textlength(text, font=font)
    if shadow: d.text(((500 - w) / 2 + 3, y + 3), text, font=font, fill=(0, 30, 30))
    d.text(((500 - w) / 2, y), text, font=font, fill=fill)
centred('MINES-PERFECT', 22, f1, (255, 255, 255))
bx, by = (500 - board.width) // 2, 100 + (330 - board.height) // 2
d.rectangle([bx + 6, by + 6, bx + board.width + 6, by + board.height + 6], fill=(0, 30, 30))
im.paste(board, (bx, by))
centred('a Minesweeper with a solver', 446, f2, (200, 235, 235), shadow=False)
im.save(sys.argv[2])
print('boxart', im.size)
