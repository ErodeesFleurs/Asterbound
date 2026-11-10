#!/usr/bin/env python3
"""Render Asterbound's ASCII artwork with Pillow and a supplied monospace font."""

import argparse
from io import BytesIO
from pathlib import Path
import struct

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
BACKGROUND = "#0c1718"
CLIENT = "#b7f5d1"
SERVER = "#9adcf1"
SIZES = (16, 24, 32, 48, 64, 128, 256)
MONOGRAM = (
    "    /\\    ",
    "   /  \\   ",
    "  / /\\ \\  ",
    " / ____ \\ ",
    "/_/    \\_\\",
)
WORDMARK = (
    r"    _    ____ _____ _____ ____  ____   ___  _   _ _   _ ____  ",
    r"   / \  / ___|_   _| ____|  _ \| __ ) / _ \| | | | \ | |  _ \ ",
    r"  / _ \ \___ \ | | |  _| | |_) |  _ \| | | | | | |  \| | | | |",
    r" / ___ \ ___) || | | |___|  _ <| |_) | |_| | |_| | |\  | |_| |",
    r"/_/   \_\____/ |_| |_____|_| \_\____/ \___/ \___/|_| \_|____/ ",
)


def draw_ascii(image, lines, font_path, box, color, shadow=False):
    """Fit a fixed character grid without stretching glyphs or trimming spaces."""
    scale = 2
    canvas = Image.new("RGBA", (image.width * scale, image.height * scale))
    draw = ImageDraw.Draw(canvas)
    left, top, right, bottom = (int(n * scale) for n in box)
    columns = max(map(len, lines))
    size = max(1, bottom - top)
    while True:
        font = ImageFont.truetype(str(font_path), size)
        cell = font.getlength("M")
        ascent, descent = font.getmetrics()
        step = ascent + descent
        if cell * columns <= right - left and step * len(lines) <= bottom - top:
            break
        size -= 1
        if size < 1:
            raise ValueError("ASCII artwork does not fit")
    x = left + (right - left - cell * columns) / 2
    y = top + (bottom - top - step * len(lines)) / 2
    for row in lines:
        draw.text((round(x), round(y)), row, font=font, fill=color, anchor="la",
                  stroke_width=2 * scale if shadow else 0, stroke_fill="#081415")
        y += step
    image.alpha_composite(canvas.resize(image.size, Image.Resampling.LANCZOS))


def icon(size, font_path, color=CLIENT):
    image = Image.new("RGBA", (size, size))
    draw = ImageDraw.Draw(image)
    inset = max(0, size // 32)
    draw.rounded_rectangle((inset, inset, size - inset - 1, size - inset - 1),
                           radius=max(2, size // 7), fill=BACKGROUND)
    if size <= 64:
        # The same /, \\ and _ characters in a hand-pixelled terminal font.
        # Three rows stay readable at 16px; shrinking the full five-row art does not.
        rows = (" /\\ ", "/__\\", "/  \\")
        height = 3 if size in (16, 32) else 5
        glyphs = {"/": ("001", "010", "100") if height == 3 else
                  ("001", "001", "010", "100", "100"),
                  "\\": ("100", "010", "001") if height == 3 else
                  ("100", "100", "010", "001", "001"),
                  "_": ("000",) * (height - 1) + ("111",)}
        grid = Image.new("RGBA", (12, height * 3 + 2))
        pixels = ImageDraw.Draw(grid)
        for row, text in enumerate(rows):
            for column, char in enumerate(text):
                for y, bits in enumerate(glyphs.get(char, ())):
                    for x, bit in enumerate(bits):
                        if bit == "1":
                            pixels.point((column * 3 + x, row * (height + 1) + y), fill=color)
        factor = max(1, min((size - 4) // grid.width, (size - 4) // grid.height))
        grid = grid.resize((grid.width * factor, grid.height * factor), Image.Resampling.NEAREST)
        image.alpha_composite(grid, ((size - grid.width) // 2, (size - grid.height) // 2))
    else:
        draw_ascii(image, MONOGRAM, font_path,
                   (size * .13, size * .12, size * .87, size * .87), color)
    return image


def write_ico(path, frames):
    # Store each deliberately drawn size, rather than auto-shrinking the master.
    payloads = []
    for image in frames:
        buffer = BytesIO()
        image.save(buffer, format="PNG")
        payloads.append(buffer.getvalue())
    offset = 6 + 16 * len(frames)
    entries = []
    for image, data in zip(frames, payloads):
        entries.append(struct.pack("<BBBBHHII", image.width % 256, image.height % 256,
                                   0, 0, 1, 32, len(data), offset))
        offset += len(data)
    path.write_bytes(struct.pack("<HHH", 0, 1, len(frames)) + b"".join(entries + payloads))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--font", type=Path, required=True, help="DejaVuSansMono.ttf")
    args = parser.parse_args()
    icon(1024, args.font).save(ROOT / "source/client/asterbound.png")
    write_ico(ROOT / "source/client/asterbound.ico", [icon(n, args.font) for n in SIZES])
    write_ico(ROOT / "source/server/asterbound_server.ico",
              [icon(n, args.font, SERVER) for n in SIZES])
    icon(16, args.font).save(ROOT / "assets/asterbound/opensb/modicon.png")
    installer = Image.new("RGBA", (138, 140), BACKGROUND)
    installer.alpha_composite(icon(128, args.font), (5, 6))
    installer.convert("RGB").save(ROOT / "scripts/inno/small.bmp")
    title = Image.new("RGBA", (1200, 420))
    draw_ascii(title, WORDMARK, args.font, (46, 90, 1154, 310), CLIENT, shadow=True)
    title.save(ROOT / "assets/asterbound/interface/title/starbound.png")
    print("Generated ASCII client/server icons, mod icon, installer art and title wordmark.")


if __name__ == "__main__":
    main()
