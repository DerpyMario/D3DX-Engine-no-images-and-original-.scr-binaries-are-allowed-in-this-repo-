#!/usr/bin/env python3
"""Decode the custom RLE bitmap resource (named type 'BMP', id 102) found in Ribbons.scr / Mystify.scr.

Header (18 bytes):
  +0  WORD   unknown (0)
  +2  DWORD  biSize-like = 10
  +6  DWORD  0
  +10 WORD   width  (128)
  +12 WORD   height (128)
  +14 WORD   bpp (24)
  +16 ...    palette/unknown (2 bytes?)
Data (from +18):
  byte >= 0x80 : run of (byte-0x80) pixels, followed by 3-byte BGR color
  byte <  0x80 : ??? maybe end-of-line / raw count
"""
import struct, sys
try:
    from PIL import Image
except ImportError:
    Image = None

def decode(path, w=128, h=128):
    data = open(path, 'rb').read()
    print('header:', data[:18].hex(' '))
    px = bytearray(w*h*3)
    i = 18
    x = y = 0
    while i < len(data) and y < h:
        op = data[i]; i += 1
        if op >= 0x80:
            n = op - 0x80
            c = data[i:i+3]; i += 3
            for _ in range(n):
                if y < h and x < w:
                    o = (y*w + x)*3
                    px[o:o+3] = c
                x += 1
        elif op == 0:
            # end of line?
            y += 1; x = 0
        elif op < 0x80:
            # maybe raw run of (op) pixels each with own color? count pixels
            for _ in range(op):
                c = data[i:i+3]; i += 3
                if y < h and x < w:
                    o = (y*w + x)*3
                    px[o:o+3] = c
                x += 1
            # maybe followed by terminator
    return bytes(px), i, len(data)

if __name__ == '__main__':
    src = sys.argv[1]
    w = int(sys.argv[2]) if len(sys.argv) > 2 else 128
    h = int(sys.argv[3]) if len(sys.argv) > 3 else 128
    px, consumed, total = decode(src, w, h)
    print(f'consumed {consumed} of {total}')
    if Image:
        img = Image.frombytes('RGB', (w, h), bytes(px), 'raw', 'BGR')
        img = img.resize((w*3, h*3), Image.NEAREST)
        out = src.replace('.bin', '.decoded.png')
        img.save(out)
        print('saved', out)
    else:
        open(src.replace('.bin', '.raw'), 'wb').write(bytes(px))
