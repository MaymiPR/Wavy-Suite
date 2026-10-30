"""Private UI check: compares a WVoxSnapshot PNG with the reference skin composed from the
local Waves install (extracted into reference/, which is git-ignored). Usage: ui_compare.py ours.png out.png"""
import os
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))     # (the repository)
import sys
from PIL import Image, ImageChops
R = os.path.join(ROOT, 'reference', 'rvox_png', 'PNG_')
base = Image.open(os.path.join(R, '15002.png')).convert('RGBA')
def put(n, x, y): base.alpha_composite(Image.open(os.path.join(R, f'{n}.png')).convert('RGBA'), (x, y))
put(11012, 135, 99)                    # comp thumb at 0 dB
put(280, 41, 105 + 195 - 41)           # gate at -80
put(280, 297, 105)                     # gain at 0
for n, x in ((10136, 50), (200, 311)): put(n, x, 121 if n == 10136 else 122)
ours = Image.open(sys.argv[1]).convert('RGBA')
ref = base.copy()
# compare only the panel area below the toolbar band
box = (0, 36, 371, 357)
d = ImageChops.difference(ref.convert('RGB').crop(box), ours.convert('RGB').crop(box)).convert('L')
import numpy as np
a = np.asarray(d, dtype=float)
print('mean abs diff (0-255): %.2f   >32: %.2f %% of pixels' % (a.mean(), 100 * (a > 32).mean()))
S = 2
sheet = Image.new('RGB', (371 * S * 3, 357 * S))
sheet.paste(ref.convert('RGB').resize((371 * S, 357 * S), Image.NEAREST), (0, 0))
sheet.paste(ours.convert('RGB').resize((371 * S, 357 * S), Image.NEAREST), (371 * S, 0))
dd = Image.new('RGB', (371, 357)); dd.paste(d.point(lambda v: min(255, v * 4)).convert('RGB'), (0, 36))
sheet.paste(dd.resize((371 * S, 357 * S), Image.NEAREST), (371 * S * 2, 0))
sheet.save(sys.argv[2])
