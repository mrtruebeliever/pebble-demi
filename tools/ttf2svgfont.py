#!/usr/bin/env python3
# Convert a TTF to a minimal SVG font containing only the digits 0-9,
# suitable as input for the pebble fctx-compiler (-> .ffont).
import sys
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen

def main(ttf_path, font_id, out_path):
    font = TTFont(ttf_path)
    upm = font["head"].unitsPerEm
    hhea = font["hhea"]
    ascent, descent = hhea.ascent, hhea.descent
    cap_height = upm  # default fallback
    if "OS/2" in font:
        os2 = font["OS/2"]
        if getattr(os2, "sCapHeight", 0):
            cap_height = os2.sCapHeight
    cmap = font.getBestCmap()
    glyph_set = font.getGlyphSet()

    glyphs = []
    for ch in "0123456789":
        cp = ord(ch)
        if cp not in cmap:
            print("WARN: codepoint %r missing" % ch, file=sys.stderr)
            continue
        gname = cmap[cp]
        g = glyph_set[gname]
        pen = SVGPathPen(glyph_set)
        g.draw(pen)
        d = pen.getCommands()
        adv = g.width
        glyphs.append((ch, adv, d))

    parts = []
    parts.append('<?xml version="1.0" standalone="no"?>')
    parts.append('<svg xmlns="http://www.w3.org/2000/svg">')
    parts.append('<defs>')
    parts.append('<font id="%s" horiz-adv-x="%d">' % (font_id, upm))
    parts.append('<font-face units-per-em="%d" ascent="%d" descent="%d" cap-height="%d"/>'
                 % (upm, ascent, descent, cap_height))
    for ch, adv, d in glyphs:
        parts.append('<glyph unicode="%s" horiz-adv-x="%d" d="%s"/>' % (ch, adv, d))
    parts.append('</font>')
    parts.append('</defs>')
    parts.append('</svg>')

    with open(out_path, "w") as f:
        f.write("\n".join(parts))
    print("wrote %s (%d glyphs, upm=%d asc=%d desc=%d cap=%d)"
          % (out_path, len(glyphs), upm, ascent, descent, cap_height))

if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], sys.argv[3])
