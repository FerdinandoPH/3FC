"""Generate the 3DS app icon and CIA banner without any image library.

The icon is a folder glyph with an up/down transfer arrow pair, drawn on a dark
slate background so it reads at 48x48 in the HOME menu.
"""
import struct
import zlib
import sys


def png(path, width, height, pixel):
    """Write an RGBA PNG. `pixel(x, y)` returns an (r, g, b, a) tuple."""
    raw = bytearray()
    for y in range(height):
        raw.append(0)  # filter: none
        for x in range(width):
            raw.extend(pixel(x, y))

    def chunk(tag, data):
        out = struct.pack(">I", len(data)) + tag + data
        return out + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", header))
        f.write(chunk(b"IDAT", zlib.compress(bytes(raw), 9)))
        f.write(chunk(b"IEND", b""))


BG_TOP = (34, 40, 54)
BG_BOTTOM = (20, 24, 34)
FOLDER = (240, 190, 90)
FOLDER_DARK = (208, 156, 62)
ARROW_UP = (120, 220, 150)
ARROW_DOWN = (110, 190, 255)


def lerp(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))


def make_pixel(size):
    """Return a pixel function for a square icon of the given size."""
    s = size / 48.0

    def rect(x, y, x0, y0, x1, y1):
        return x0 <= x < x1 and y0 <= y < y1

    def pixel(x, y):
        bg = lerp(BG_TOP, BG_BOTTOM, y / max(size - 1, 1))

        fx, fy = x / s, y / s  # work in 48x48 space

        # folder body
        if rect(fx, fy, 6, 15, 42, 39):
            colour = FOLDER
            # a darker lip along the top edge gives the folder some depth
            if fy < 18:
                colour = FOLDER_DARK
            return colour + (255,)

        # folder tab
        if rect(fx, fy, 6, 11, 21, 15):
            return FOLDER_DARK + (255,)

        return bg + (255,)

    def with_arrows(x, y):
        s2 = size / 48.0
        fx, fy = x / s2, y / s2
        base = pixel(x, y)

        # only draw the arrows over the folder body
        if not (6 <= fx < 42 and 15 <= fy < 39):
            return base

        # up arrow on the left, down arrow on the right
        def arrow(cx, top, bottom, up):
            half = (bottom - top) * 0.45
            if up:
                # head
                if top <= fy < top + half and abs(fx - cx) <= (fy - top) * 0.9:
                    return True
            else:
                if bottom - half <= fy < bottom and abs(fx - cx) <= (bottom - fy) * 0.9:
                    return True
            # shaft
            if top <= fy < bottom and abs(fx - cx) <= 1.6:
                return True
            return False

        if arrow(17, 19, 35, True):
            return ARROW_UP + (255,)
        if arrow(31, 19, 35, False):
            return ARROW_DOWN + (255,)

        return base

    return with_arrows


def banner_pixel(x, y):
    """256x128 CIA banner: the icon centred on a gradient field."""
    bg = lerp(BG_TOP, BG_BOTTOM, y / 127.0)

    # a centred 104x104 icon; without a font to set the name in, a clean
    # centred mark reads better than a logo pushed to one side
    size = 104
    x0 = (256 - size) // 2
    y0 = (128 - size) // 2
    if x0 <= x < x0 + size and y0 <= y < y0 + size:
        return make_pixel(size)(x - x0, y - y0)

    return bg + (255,)


if __name__ == "__main__":
    png(sys.argv[1], 48, 48, make_pixel(48))
    png(sys.argv[2], 256, 128, banner_pixel)
    print("written")
