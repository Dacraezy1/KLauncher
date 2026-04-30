#!/usr/bin/env python3
"""Generate placeholder icons for KLauncher using only stdlib."""
import struct, zlib, os

def png_chunk(name, data):
    c = zlib.crc32(name + data) & 0xffffffff
    return struct.pack('>I', len(data)) + name + data + struct.pack('>I', c)

def make_png(w, h, pixels_rgba):
    sig = b'\x89PNG\r\n\x1a\n'
    ihdr_data = struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)
    ihdr = png_chunk(b'IHDR', ihdr_data)
    raw_rows = b''
    for y in range(h):
        raw_rows += b'\x00'
        for x in range(w):
            r,g,b,a = pixels_rgba[y*w+x]
            raw_rows += bytes([r,g,b])
    idat = png_chunk(b'IDAT', zlib.compress(raw_rows))
    iend = png_chunk(b'IEND', b'')
    return sig + ihdr + idat + iend

def circle_icon(w, h, bg, fg):
    pixels = []
    cx, cy, r = w//2, h//2, w//2 - 2
    for y in range(h):
        for x in range(w):
            dx, dy = x-cx, y-cy
            if dx*dx + dy*dy <= r*r:
                # K letter glyph
                lx = x - cx + r//2
                ly = y - cy
                if abs(lx) <= 3:
                    pixels.append(fg)
                elif ly < 0 and lx > 0 and abs(lx - (-ly)) <= 3:
                    pixels.append(fg)
                elif ly >= 0 and lx > 0 and abs(lx - ly) <= 3:
                    pixels.append(fg)
                else:
                    pixels.append(bg)
            else:
                pixels.append((0,0,0,0))
    return pixels

# 256x256 app icon
W = 256
bg = (15, 52, 96, 255)
fg = (79, 195, 247, 255)
pix = circle_icon(W, W, bg, fg)
data = make_png(W, W, pix)
os.makedirs('resources/icons', exist_ok=True)
with open('resources/icons/klauncher.png', 'wb') as f:
    f.write(data)

# 600x350 splash
SW, SH = 600, 350
splash_pix = []
for y in range(SH):
    for x in range(SW):
        t = x / SW
        r = int(15*(1-t) + 26*t)
        g = int(20*(1-t) + 26*t)
        b = int(30*(1-t) + 46*t)
        splash_pix.append((r,g,b,255))
os.makedirs('resources/images', exist_ok=True)
with open('resources/images/splash.png', 'wb') as f:
    f.write(make_png(SW, SH, splash_pix))

print("Icons generated.")
