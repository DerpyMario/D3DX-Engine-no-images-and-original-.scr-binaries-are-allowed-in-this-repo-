#!/usr/bin/env python3
"""Wrap raw RT_ICON DIB resources into proper multi-frame .ico files.

RT_ICON resources contain the bare image data (BITMAPINFOHEADER + pixels, or PNG).
A .ico file needs a 6-byte ICONDIR + 16-byte ICONDIRENTRY per frame.
"""
import struct, sys, os

def read_icon_frame(data):
    """Return (width, height, bpp, colors) from an RT_ICON frame."""
    if data[:8] == b'\x89PNG\r\n\x1a\n':
        w, h = struct.unpack_from('>II', data, 16)
        return w, h, 32, 0
    biSize, w, h = struct.unpack_from('<Iii', data, 0)
    planes, bpp = struct.unpack_from('<HH', data, 12)
    # RT_ICON DIBs are stored double-height (XOR mask + AND mask)
    disp_h = h // 2 if h > 0 else h
    colors = (1 << bpp) if bpp <= 8 else 0
    return w, disp_h, bpp, colors

def build_ico(frames, out_path):
    n = len(frames)
    entries = []
    for data in frames:
        w, h, bpp, colors = read_icon_frame(data)
        entries.append((w, h, colors, 1, bpp, len(data)))
    offset = 6 + 16 * n
    out = bytearray()
    out += struct.pack('<HHH', 0, 1, n)   # ICONDIR: reserved, type=1 (icon), count
    for (w, h, colors, planes, bpp, size) in entries:
        out += struct.pack('<BBBBHHII', w & 0xFF, h & 0xFF, colors & 0xFF, 0,
                           planes, bpp, size, offset)
        offset += size
    for data in frames:
        out += data
    open(out_path, 'wb').write(bytes(out))
    print(f'{out_path}: {n} frames, {len(out)} bytes')

if __name__ == '__main__':
    resdir = sys.argv[1]
    frames = []
    for name in ('1_lang1033.ico', '2_lang1033.ico'):
        frames.append(open(os.path.join(resdir, 'ICON', name), 'rb').read())
    build_ico(frames, os.path.join(resdir, 'ICON', f'group_101.ico'))
