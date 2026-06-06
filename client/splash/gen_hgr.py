#!/usr/bin/env python3
"""Convert fujiblogger-logo.png to logo.hgr (8192-byte Apple II HGR page).

Usage:
    python3 gen_hgr.py                       # fujiblogger-logo.png -> logo.hgr
    python3 gen_hgr.py input.png output.hgr  # explicit paths

Output: 8192 bytes, Apple II HGR interleaved layout.
280 x 192 pixels, 7 pixels per byte (bit 7 = 0, green-white palette).
Dark source pixels -> lit (1); light source pixels -> dark (0).

Layout (full-screen HGR):
  Logo image scaled into the top LOGO_H scanlines, centred.
  "FUJIBLOGGER V<ver>"  centred below the logo.
  "PRESS ANY KEY..."    centred near the bottom.
"""

import sys
from pathlib import Path

try:
    from PIL import Image, ImageDraw, ImageEnhance, ImageFilter, ImageFont
except ImportError:
    sys.exit("Pillow required:  pip install Pillow")

HR_SIZE = 8192
HGR_W = 280
HGR_H = 192
LOGO_H = 130    # logo occupies the top 130 scanlines; text goes below


def hr_offset(y):
    """Byte offset for scanline y (0-191) in the HGR page."""
    thirds = (0, 40, 80)
    return ((y & 7) << 10) | (((y >> 3) & 7) << 7) | thirds[y >> 6]


def build_font(size):
    """Return a bold truetype font, falling back to PIL's default."""
    for name in ("DejaVuSansMono-Bold.ttf", "DejaVuSans-Bold.ttf",
                 "LiberationMono-Bold.ttf", "FreeMonoBold.ttf"):
        for search in ("/usr/share/fonts", "/usr/local/share/fonts",
                       str(Path.home() / ".fonts")):
            base = Path(search)
            if not base.is_dir():
                continue
            for p in base.rglob(name):
                try:
                    return ImageFont.truetype(str(p), size)
                except Exception:
                    pass
    return ImageFont.load_default()


def draw_centred(canvas, text, y_top, font):
    """Draw black text horizontally centred at y_top on a white canvas."""
    draw = ImageDraw.Draw(canvas)
    bbox = draw.textbbox((0, 0), text, font=font)
    x = (HGR_W - (bbox[2] - bbox[0])) // 2
    draw.text((x, y_top), text, fill=0, font=font)


def png_to_hgr(src_path, version=""):
    # --- Logo centred in the top LOGO_H scanlines --------------------------
    src = Image.open(src_path).convert("RGBA")
    bg = Image.new("RGBA", src.size, (255, 255, 255, 255))
    img = Image.alpha_composite(bg, src).convert("L")

    img.thumbnail((HGR_W, LOGO_H), Image.LANCZOS)
    w, h = img.size

    canvas = Image.new("L", (HGR_W, HGR_H), 255)   # white = off in HGR
    canvas.paste(img, ((HGR_W - w) // 2, (LOGO_H - h) // 2))

    # --- Sharpen + contrast the logo area, then add crisp text below ------
    logo_area = canvas.crop((0, 0, HGR_W, LOGO_H))
    logo_area = logo_area.filter(ImageFilter.SHARPEN)
    logo_area = ImageEnhance.Contrast(logo_area).enhance(2.5)
    canvas.paste(logo_area, (0, 0))

    ver_str = ("FUJIBLOGGER V" + version) if version else "FUJIBLOGGER"
    draw_centred(canvas, ver_str,          145, build_font(13))
    draw_centred(canvas, "PRESS ANY KEY...", 172, build_font(11))

    mono = canvas.convert("1", dither=Image.FLOYDSTEINBERG)

    # --- Pack into HGR page -----------------------------------------------
    page = bytearray(HR_SIZE)
    for y in range(HGR_H):
        row_off = hr_offset(y)
        for x in range(HGR_W):
            if mono.getpixel((x, y)) == 0:      # dark -> lit on phosphor
                page[row_off + x // 7] |= 1 << (x % 7)

    return bytes(page)


if __name__ == "__main__":
    src_path = (
        Path(sys.argv[1]) if len(sys.argv) > 1
        else Path(__file__).parent / "fujiblogger-logo.png"
    )
    dst_path = (
        Path(sys.argv[2]) if len(sys.argv) > 2
        else Path(__file__).parent / "logo.hgr"
    )
    version = sys.argv[3] if len(sys.argv) > 3 else ""

    data = png_to_hgr(src_path, version)
    dst_path.write_bytes(data)
    print(f"Wrote {len(data)} bytes -> {dst_path}", file=sys.stderr)
