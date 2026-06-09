#!/usr/bin/env python3
# Rasterize a monospace TTF into a packed grayscale (alpha) atlas for the
# bootloader UI. Output is a C header with one byte per pixel (0..255 coverage),
# ASCII 32..126, fixed cell. Anti-aliased text is then alpha-blended on device.
import sys
from PIL import Image, ImageDraw, ImageFont

TTF = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
SIZE = 22
FIRST, LAST = 32, 126

font = ImageFont.truetype(TTF, SIZE)
ascent, descent = font.getmetrics()
cell_h = ascent + descent
# monospace advance width
adv = int(round(font.getlength("M")))
cell_w = adv

count = LAST - FIRST + 1
atlas = bytearray(count * cell_w * cell_h)
for i in range(count):
    ch = chr(FIRST + i)
    img = Image.new("L", (cell_w, cell_h), 0)
    d = ImageDraw.Draw(img)
    d.text((0, 0), ch, font=font, fill=255)
    px = img.load()
    base = i * cell_w * cell_h
    for y in range(cell_h):
        for x in range(cell_w):
            atlas[base + y * cell_w + x] = px[x, y]

out = []
out.append("/* Auto-generated grayscale font atlas (DejaVu Sans Mono). Do not edit. */")
out.append("#ifndef FASTBOOT_UI_FONT_H")
out.append("#define FASTBOOT_UI_FONT_H")
out.append(f"#define FONT_FIRST_CHAR {FIRST}")
out.append(f"#define FONT_LAST_CHAR  {LAST}")
out.append(f"#define FONT_CELL_W {cell_w}")
out.append(f"#define FONT_CELL_H {cell_h}")
out.append(f"#define FONT_GLYPH_COUNT {count}")
out.append("STATIC CONST UINT8 gFontAtlas[FONT_GLYPH_COUNT * FONT_CELL_W * FONT_CELL_H] = {")
line = "  "
for n, b in enumerate(atlas):
    line += f"{b},"
    if len(line) >= 100:
        out.append(line)
        line = "  "
if line.strip():
    out.append(line)
out.append("};")
out.append("#endif")

sys.stdout.write("\n".join(out) + "\n")
sys.stderr.write(f"cell={cell_w}x{cell_h} glyphs={count} bytes={len(atlas)}\n")
