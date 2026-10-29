"""Generate the RmlUi menus' decorative images into resources/rml/images.

Run from anywhere:  python tools/rml_art.py

Pure Python (zlib + struct), no imaging library, so it runs on any checkout.
Everything is drawn from geometry with supersampling, and every image is
straight (not premultiplied) RGBA, which is what the RmlUi backend expects.

The look is Fatal Frame's: an old photograph's sepia over black, film grain
over everything, and the warm gold of the game's menu dividers.  The film
strip across the options screen's top is the game's own, read from the disc
at run time, so it is not drawn here.
"""

import math
import os
import random
import struct
import zlib

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..",
                   "resources", "rml", "images")


def write_png(path, width, height, pixels):
    """pixels: flat list of (r, g, b, a) tuples, 0..255."""
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        for x in range(width):
            raw.extend(pixels[y * width + x])

    def chunk(tag, data):
        body = tag + data
        return (struct.pack(">I", len(data)) + body +
                struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF))

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def clamp(v, lo=0.0, hi=1.0):
    return lo if v < lo else hi if v > hi else v


def lerp(a, b, t):
    return a + (b - a) * t


def blur(values, width, height, radius):
    """Three box passes each way: close enough to a gaussian."""
    for _ in range(3):
        out = [0.0] * (width * height)
        for y in range(height):
            row = y * width
            acc = 0.0
            window = 2 * radius + 1
            for x in range(-radius, radius + 1):
                acc += values[row + min(max(x, 0), width - 1)]
            for x in range(width):
                out[row + x] = acc / window
                acc += values[row + min(x + radius + 1, width - 1)]
                acc -= values[row + max(x - radius, 0)]
        values = out
        out = [0.0] * (width * height)
        for x in range(width):
            acc = 0.0
            window = 2 * radius + 1
            for y in range(-radius, radius + 1):
                acc += values[min(max(y, 0), height - 1) * width + x]
            for y in range(height):
                out[y * width + x] = acc / window
                acc += values[min(y + radius + 1, height - 1) * width + x]
                acc -= values[max(y - radius, 0) * width + x]
        values = out
    return values


# ---------------------------------------------------------------------------
#  The rest.
# ---------------------------------------------------------------------------

def vignette(size=256):
    """Black at the edges, clear in the middle, the brown of an old print
    between -- stretched over the whole screen behind a menu."""
    pixels = []
    for y in range(size):
        for x in range(size):
            dx = (x + 0.5) / size * 2.0 - 1.0
            dy = (y + 0.5) / size * 2.0 - 1.0
            d = math.sqrt(dx * dx * 0.85 + dy * dy)
            a = clamp((d - 0.30) / 1.0) ** 1.5
            warm = clamp(1.0 - abs(d - 0.95) / 0.5) * 0.45
            pixels.append((int(46 * warm), int(28 * warm), int(12 * warm),
                           int(a * 240)))
    return size, size, pixels


def grain(size=128, seed=2001):
    """Film grain to tile over a menu: dark speckle with the odd pale fleck,
    the way the game's own picture crawls."""
    rng = random.Random(seed)
    pixels = []
    for _ in range(size * size):
        v = rng.random()
        if v < 0.45:
            pixels.append((0, 0, 0, int(rng.random() ** 3 * 44)))
        elif v < 0.6:
            pixels.append((236, 218, 186, int(rng.random() ** 5 * 22)))
        else:
            pixels.append((0, 0, 0, 0))
    return size, size, pixels


def _cursor(width, height, horizontal_of):
    """The menu cursor: a band of warm umber with a pale candlelit core,
    between two thin gold rails, all fading out along the row -- the bar
    the title menu lights under its entries."""
    pixels = []
    for y in range(height):
        v = (y + 0.5) / height * 2.0 - 1.0
        body = math.exp(-(v / 0.62) ** 2) * 0.78
        core = math.exp(-(v / 0.16) ** 2)
        rail = max(clamp(1.0 - abs(y - 1.0) / 1.0),
                   clamp(1.0 - abs(y - (height - 2.0)) / 1.0))
        for x in range(width):
            h = horizontal_of((x + 0.5) / width)
            b_a = body * h
            c_a = core * h * 0.45
            r_a = rail * clamp(h * 1.25) * 0.85
            total = b_a + c_a + r_a
            a = clamp(total)
            if a <= 0.0:
                pixels.append((0, 0, 0, 0))
                continue
            # umber body, candle core, gold rails
            r = (122 * b_a + 246 * c_a + 214 * r_a) / total
            g = (80 * b_a + 214 * c_a + 172 * r_a) / total
            bl = (32 * b_a + 150 * c_a + 98 * r_a) / total
            pixels.append((int(r), int(g), int(bl), int(a * 255)))
    return width, height, pixels


def cursor_glow(width=256, height=32):
    """For a row read from the left: brightest at the left, dying away to the
    right."""
    return _cursor(width, height,
                   lambda u: clamp(1.0 - u) ** 1.2 * clamp(u / 0.03))


def cursor_bar(width=256, height=32):
    """For a centred item: brightest in the middle, dying away to both ends."""
    return _cursor(width, height,
                   lambda u: clamp(1.0 - abs(u * 2.0 - 1.0)) ** 1.1)


def rule(width=1120, height=12):
    """A gold hairline that fades at both ends, with a small diamond at its
    centre -- the line the title menu draws under its entries.  Drawn at the
    shape it is shown at (560 x 6 dp) so the diamond is not stretched."""
    pixels = []
    cy = (height - 1) / 2.0
    for y in range(height):
        for x in range(width):
            u = (x + 0.5) / width * 2.0 - 1.0
            fade = clamp(1.0 - abs(u)) ** 0.8
            line = clamp(1.0 - abs(y - cy) / 0.9)
            dx = abs(x + 0.5 - width / 2.0)
            dy = abs(y + 0.5 - height / 2.0)
            diamond = clamp((4.5 - (dx * 0.8 + dy * 1.25)) / 1.0)
            a = max(line * fade * 0.85, diamond)
            r = lerp(179, 240, diamond)
            g = lerp(147, 206, diamond)
            b = lerp(96, 140, diamond)
            pixels.append((int(r), int(g), int(b), int(clamp(a) * 255)))
    return width, height, pixels


def main():
    os.makedirs(OUT, exist_ok=True)
    for name, image in (("vignette.png", vignette()),
                        ("grain.png", grain()),
                        ("cursor_glow.png", cursor_glow()),
                        ("cursor_bar.png", cursor_bar()),
                        ("rule.png", rule())):
        width, height, pixels = image
        write_png(os.path.join(OUT, name), width, height, pixels)
        print("wrote", name, width, "x", height)


if __name__ == "__main__":
    main()
