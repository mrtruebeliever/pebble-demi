#!/usr/bin/env python3
"""Generates Demi's appstore banner: the three supported watches side by side,
each wearing a different configuration of the face.

Input (emulator screenshots, see store/banner-src/):
  time2.png    Pebble Time 2 (emery, 200x228)   light theme, two bars
  round2.png   Pebble Round 2 (gabbro, 260x260) dark, orange, daylight bar
  duo.png      Pebble 2 Duo (flint, 144x168)    black and white, horizontal

Output:
  store/banner-720x320.png

The watch bodies are drawn, not photographed: simple shapes in the spirit of
each model, so the faces carry the picture. Everything is drawn at 3x and
scaled down for smooth edges.

Run from the project root (needs Pillow):  python3 tools/gen_banner.py
"""
import os

from PIL import Image, ImageDraw, ImageFont, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "store", "banner-src")
OUT = os.path.join(ROOT, "store", "banner-720x320.png")
FONT_BOLD = os.path.join(ROOT, "resources", "fonts", "Rajdhani-Bold.ttf")
FONT_LIGHT = os.path.join(ROOT, "resources", "fonts", "Rajdhani-Light.ttf")

K = 3                      # supersampling factor
W, H = 720 * K, 320 * K

BG_TOP = (24, 26, 30)
BG_BOTTOM = (8, 9, 11)
ACCENT = (0x00, 0xFF, 0x7F)      # Demi's default green
TEXT = (240, 240, 240)
DIM = (150, 154, 160)


def background():
    img = Image.new("RGB", (W, H))
    d = ImageDraw.Draw(img)
    for y in range(H):
        t = y / (H - 1)
        c = tuple(int(BG_TOP[i] * (1 - t) + BG_BOTTOM[i] * t) for i in range(3))
        d.line([(0, y), (W, y)], fill=c)
    return img


def shadow(img, box, radius, ellipse=False):
    """A soft drop shadow under a watch body."""
    sh = Image.new("L", img.size, 0)
    d = ImageDraw.Draw(sh)
    x0, y0, x1, y1 = box
    off = 10 * K
    if ellipse:
        d.ellipse((x0, y0 + off, x1, y1 + off), fill=150)
    else:
        d.rounded_rectangle((x0, y0 + off, x1, y1 + off), radius, fill=150)
    sh = sh.filter(ImageFilter.GaussianBlur(14 * K))
    img.paste((0, 0, 0), (0, 0), sh)


def screen(name, scale):
    s = Image.open(os.path.join(SRC, name)).convert("RGB")
    # NEAREST keeps the face's pixels crisp, the way the watch shows them.
    return s.resize((round(s.width * scale * K), round(s.height * scale * K)), Image.NEAREST)


def rect_watch(img, cx, cy, scr, body, bezel, pad_x, pad_top, pad_bottom, radius, buttons):
    sw, sh = scr.size
    x0, y0 = cx - sw // 2 - pad_x, cy - sh // 2 - pad_top
    x1, y1 = cx + sw // 2 + pad_x, cy + sh // 2 + pad_bottom
    d = ImageDraw.Draw(img)
    for side, ys in buttons.items():
        for by in ys:
            bx0 = x0 - 5 * K if side == "left" else x1 - 2 * K
            d.rounded_rectangle((bx0, cy + by - 14 * K, bx0 + 7 * K, cy + by + 14 * K), 3 * K, fill=body[1])
    shadow(img, (x0, y0, x1, y1), radius)
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((x0, y0, x1, y1), radius, fill=body[0])
    d.rounded_rectangle((x0 + 3 * K, y0 + 3 * K, x1 - 3 * K, y1 - 3 * K), radius - 3 * K, fill=bezel)
    img.paste(scr, (cx - sw // 2, cy - sh // 2))
    return (x0, y0, x1, y1)


def round_watch(img, cx, cy, scr, body, bezel, ring):
    sw = scr.size[0]
    r_scr = sw // 2
    r_body = r_scr + ring
    d = ImageDraw.Draw(img)
    for by in (-38, 0, 38):
        d.rounded_rectangle((cx + r_body - 6 * K, cy + by * K - 13 * K, cx + r_body + 6 * K, cy + by * K + 13 * K),
                            3 * K, fill=body[1])
    d.rounded_rectangle((cx - r_body - 6 * K, cy - 13 * K, cx - r_body + 6 * K, cy + 13 * K), 3 * K, fill=body[1])
    box = (cx - r_body, cy - r_body, cx + r_body, cy + r_body)
    shadow(img, box, 0, ellipse=True)
    d = ImageDraw.Draw(img)
    d.ellipse(box, fill=body[0])
    d.ellipse((box[0] + 4 * K, box[1] + 4 * K, box[2] - 4 * K, box[3] - 4 * K), fill=bezel)
    mask = Image.new("L", scr.size, 0)
    ImageDraw.Draw(mask).ellipse((0, 0, sw - 1, sw - 1), fill=255)
    img.paste(scr, (cx - r_scr, cy - r_scr), mask)
    return box


def label(img, cx, y, s):
    d = ImageDraw.Draw(img)
    f = ImageFont.truetype(FONT_LIGHT, 16 * K)
    w = d.textlength(s, font=f)
    d.text((cx - w / 2, y), s, font=f, fill=DIM)


def main():
    img = background()
    d = ImageDraw.Draw(img)

    # Title block on the left.
    d.text((26 * K, 84 * K), "Demi", font=ImageFont.truetype(FONT_BOLD, 76 * K), fill=TEXT)
    d.rectangle((31 * K, 178 * K, 120 * K, 182 * K), fill=ACCENT)
    d.rectangle((120 * K, 178 * K, 160 * K, 182 * K), fill=(70, 74, 80))
    ft = ImageFont.truetype(FONT_LIGHT, 18 * K)
    d.text((31 * K, 192 * K), "Big digits, your bars,", font=ft, fill=DIM)
    d.text((31 * K, 212 * K), "your widgets.", font=ft, fill=DIM)

    base = 262 * K          # labels sit on this line

    # Pebble 2 Duo: small, black plastic, buttons on both sides.
    duo = screen("duo.png", 0.82)
    b = rect_watch(img, 266 * K, 148 * K, duo, body=((50, 52, 56), (66, 68, 72)), bezel=(14, 14, 16),
                   pad_x=12 * K, pad_top=26 * K, pad_bottom=26 * K, radius=18 * K,
                   buttons={"left": [-26 * K], "right": [-40 * K, 0, 40 * K]})
    label(img, 266 * K, base, "Pebble 2 Duo")

    # Pebble Time 2: the largest screen, a steel frame.
    t2 = screen("time2.png", 0.76)
    rect_watch(img, 438 * K, 146 * K, t2, body=((150, 154, 160), (120, 124, 130)), bezel=(12, 12, 14),
               pad_x=13 * K, pad_top=15 * K, pad_bottom=15 * K, radius=22 * K,
               buttons={"left": [-20 * K], "right": [-42 * K, 0, 42 * K]})
    label(img, 438 * K, base, "Pebble Time 2")

    # Pebble Round 2: round, a thin polished ring.
    r2 = screen("round2.png", 0.6)
    round_watch(img, 612 * K, 148 * K, r2, body=((176, 160, 140), (140, 126, 108)), bezel=(10, 10, 12),
                ring=10 * K)
    label(img, 612 * K, base, "Pebble Round 2")

    img.resize((720, 320), Image.LANCZOS).save(OUT)
    print("wrote", OUT)


if __name__ == "__main__":
    main()
