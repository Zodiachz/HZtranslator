#!/usr/bin/env python3
"""Regenerates the HYPERS TRANSLATOR icon set in resources/icons/.

Requires Pillow (pip install pillow). The output is committed to the repo, so
this script only has to be run when the artwork changes.

    python tools/generate_icons.py
"""
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "resources" / "icons"

RED = (229, 38, 46, 255)
RED_DARK = (150, 18, 26, 255)
GREY = (110, 110, 122, 255)
GREY_DARK = (58, 58, 66, 255)
WHITE = (255, 255, 255, 255)

MASTER = 1024  # draw large, then downsample for crisp small sizes


def draw_master(top, bottom) -> Image.Image:
    img = Image.new("RGBA", (MASTER, MASTER), (0, 0, 0, 0))

    # Vertical gradient rounded square.
    grad = Image.new("RGBA", (MASTER, MASTER))
    gd = ImageDraw.Draw(grad)
    for y in range(MASTER):
        t = y / (MASTER - 1)
        color = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(4))
        gd.line([(0, y), (MASTER, y)], fill=color)
    mask = Image.new("L", (MASTER, MASTER), 0)
    ImageDraw.Draw(mask).rounded_rectangle(
        [40, 40, MASTER - 40, MASTER - 40], radius=230, fill=255)
    img.paste(grad, (0, 0), mask)

    d = ImageDraw.Draw(img)
    # Stylised "H": two pillars and a crossbar shaped like a double arrow
    # (the "translate" motif).
    pillar_w = 150
    d.rounded_rectangle([250, 230, 250 + pillar_w, 794], radius=40, fill=WHITE)
    d.rounded_rectangle([MASTER - 250 - pillar_w, 230, MASTER - 250, 794], radius=40, fill=WHITE)

    bar_top, bar_bottom = 452, 572
    d.rectangle([250 + pillar_w - 10, bar_top, MASTER - 250 - pillar_w + 10, bar_bottom], fill=WHITE)
    # Arrow heads poking out on both sides of the crossbar.
    mid = (bar_top + bar_bottom) // 2
    d.polygon([(250 - 10, mid - 130), (110, mid), (250 - 10, mid + 130)], fill=WHITE)
    d.polygon([(MASTER - 250 + 10, mid - 130), (MASTER - 110, mid), (MASTER - 250 + 10, mid + 130)],
              fill=WHITE)
    return img


def draw_glyph(kind: str, size: int, color) -> Image.Image:
    """Small UI glyphs for the stylesheet (QSS can't draw arrows/check marks itself)."""
    scale = 8
    big = size * scale
    img = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    w = max(2, int(big * 0.13))
    if kind == "chevron-down":
        pts = [(big * 0.22, big * 0.38), (big * 0.5, big * 0.66), (big * 0.78, big * 0.38)]
    elif kind == "chevron-up":
        pts = [(big * 0.22, big * 0.62), (big * 0.5, big * 0.34), (big * 0.78, big * 0.62)]
    else:  # check
        pts = [(big * 0.2, big * 0.52), (big * 0.42, big * 0.72), (big * 0.8, big * 0.3)]
    d.line(pts, fill=color, width=w, joint="curve")
    for x, y in (pts[0], pts[-1]):
        d.ellipse([x - w / 2, y - w / 2, x + w / 2, y + w / 2], fill=color)
    return img.resize((size, size), Image.LANCZOS)


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    master = draw_master(RED, RED_DARK)
    sizes = [16, 24, 32, 48, 64, 128, 256]
    for s in sizes:
        master.resize((s, s), Image.LANCZOS).save(OUT / f"app-{s}.png")
    master.save(OUT / "app.ico", sizes=[(s, s) for s in sizes])

    paused = draw_master(GREY, GREY_DARK)
    paused.resize((32, 32), Image.LANCZOS).save(OUT / "tray-paused-32.png")
    glyph_color = (200, 200, 210, 255)
    for kind, color in (("chevron-down", glyph_color), ("chevron-up", glyph_color), ("check", WHITE)):
        draw_glyph(kind, 12, color).save(OUT / f"{kind}.png")
        draw_glyph(kind, 24, color).save(OUT / f"{kind}@2x.png")
    print(f"Icons written to {OUT}")


if __name__ == "__main__":
    main()
