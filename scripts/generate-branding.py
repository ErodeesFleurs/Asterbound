#!/usr/bin/env python3
"""Generate platform icons from the supplied pixel artwork."""
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SIZES = (16, 24, 32, 48, 64, 128, 256)


def icon(source, size):
    height = round(size * source.height / max(source.size))
    width = round(size * source.width / max(source.size))
    scaled = source.resize((width, height), Image.Resampling.NEAREST)
    frame = Image.new('RGBA', (size, size))
    frame.paste(scaled, ((size - width) // 2, (size - height) // 2))
    return frame


def main():
    with Image.open(ROOT / 'scripts/branding/icon.png') as source:
        icon(source, 1024).save(ROOT / 'source/client/asterbound.png')
        frames = [icon(source, n) for n in SIZES]
        for path in ('source/client/asterbound.ico', 'source/server/asterbound_server.ico'):
            frames[-1].save(ROOT / path, sizes=[f.size for f in frames], append_images=frames[:-1])
        frames[0].save(ROOT / 'assets/asterbound/asterbound/modicon.png')
        bitmap = Image.new('RGB', (138, 140), '#161821')
        image = icon(source, 128)
        bitmap.paste(image, (5, 6), image)
        bitmap.save(ROOT / 'scripts/inno/small.bmp')


if __name__ == '__main__':
    main()
