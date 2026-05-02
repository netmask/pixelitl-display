#!/usr/bin/env python3
"""Take a PNG path, output palette-indexed sprite JSON on stdout.

Usage:
    python3 rasterize_stdin.py INPUT.png [--size 48] [--palette 10] [--alpha-threshold 128]

Stdout:
    {
      "w": 48, "h": 48,
      "palette": [0, 65535, ...],   # RGB565 ints; index 0 is transparent (0)
      "data":    [0, 0, 1, 2, ...]  # row-major byte indices (w*h entries)
    }
"""
import argparse
import json
import sys
from PIL import Image


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("input")
    ap.add_argument("--size", type=int, default=48)
    ap.add_argument("--palette", type=int, default=10)
    ap.add_argument("--alpha-threshold", type=int, default=128)
    args = ap.parse_args()

    size = args.size
    max_pal = max(2, args.palette)
    src = Image.open(args.input).convert("RGBA")

    sw, sh = src.size
    scale = size / max(sw, sh)
    new_w, new_h = max(1, int(sw * scale)), max(1, int(sh * scale))
    resized = src.resize((new_w, new_h), Image.LANCZOS)
    canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    canvas.paste(resized, ((size - new_w) // 2, (size - new_h) // 2), resized)

    rgb = Image.new("RGB", (size, size), (0, 0, 0))
    rgb.paste(canvas, mask=canvas.split()[3])
    alpha = canvas.split()[3]

    opaque_pixels = []
    for y in range(size):
        for x in range(size):
            if alpha.getpixel((x, y)) >= args.alpha_threshold:
                opaque_pixels.append(rgb.getpixel((x, y)))

    if not opaque_pixels:
        # fully transparent input — still return a valid sprite
        pal = [0]
        data = [0] * (size * size)
        json.dump({"w": size, "h": size, "palette": pal, "data": data}, sys.stdout)
        return

    qhelper = Image.new("RGB", (len(opaque_pixels), 1))
    qhelper.putdata(opaque_pixels)
    qhelper = qhelper.quantize(colors=max_pal - 1, method=Image.MEDIANCUT)
    q_pal = qhelper.getpalette()[: (max_pal - 1) * 3]

    pal_rgb565 = [0]  # index 0 = transparent
    for i in range(max_pal - 1):
        if i * 3 + 2 >= len(q_pal):
            break
        r, g, b = q_pal[i * 3], q_pal[i * 3 + 1], q_pal[i * 3 + 2]
        pal_rgb565.append(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3))

    # Nearest neighbor against the selected palette (skip transparent)
    def nearest(r, g, b):
        best, best_d = 1, 10 ** 9
        for idx in range(1, len(pal_rgb565)):
            rgb565 = pal_rgb565[idx]
            pr = ((rgb565 >> 11) & 0x1F) << 3
            pg = ((rgb565 >> 5) & 0x3F) << 2
            pb = (rgb565 & 0x1F) << 3
            d = (r - pr) ** 2 + (g - pg) ** 2 + (b - pb) ** 2
            if d < best_d:
                best, best_d = idx, d
        return best

    data = []
    for y in range(size):
        for x in range(size):
            if alpha.getpixel((x, y)) < args.alpha_threshold:
                data.append(0)
            else:
                r, g, b = rgb.getpixel((x, y))
                data.append(nearest(r, g, b))

    json.dump({"w": size, "h": size, "palette": pal_rgb565, "data": data}, sys.stdout)


if __name__ == "__main__":
    main()
