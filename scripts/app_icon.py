#!/usr/bin/env python3
"""Builds the square app icon used for Android from exemples/logo.svg.

Android launchers draw "adaptive" icons: they crop roughly the outer 40% of
the image and fill the rest with a background, so a logo that touches the
edges ends up cropped to its centre. This wraps the logo on a black square
(#0b0b0b, the site's background), scaled into the zone that's always shown.

    python3 scripts/app_icon.py out.svg [scale]   # scale: share of the side, default 0.58
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
out = Path(sys.argv[1])
scale = float(sys.argv[2]) if len(sys.argv) > 2 else 0.58

svg = (ROOT / "exemples" / "logo.svg").read_text(encoding="utf-8")
open_tag = re.search(r"<svg\b[^>]*>", svg)
box = re.search(r'viewBox="([\d.\s-]+)"', open_tag.group(0))
x, y, w, h = (float(v) for v in box.group(1).split()) if box else (0, 0, 300, 300)
inner = svg[open_tag.end():svg.rindex("</svg>")]
inner = re.sub(r"<!--.*?-->|<title>.*?</title>|<desc>.*?</desc>|<metadata>.*?</metadata>", "", inner, flags=re.S)

size = 1024
s = size * scale / max(w, h)
dx = (size - w * s) / 2 - x * s
dy = (size - h * s) / 2 - y * s
out.write_text(
    f'<svg xmlns="http://www.w3.org/2000/svg" width="{size}" height="{size}" viewBox="0 0 {size} {size}">'
    f'<rect width="{size}" height="{size}" fill="#0b0b0b"/>'
    f'<g transform="translate({dx:.3f} {dy:.3f}) scale({s:.5f})">{inner}</g></svg>',
    encoding="utf-8",
)
print(f"{out}: logo at {scale:.0%} on #0b0b0b")
