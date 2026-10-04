#!/usr/bin/env python3
"""Pure Python pixel/row model; no compiler or C++ intrinsic execution."""
import random

rng = random.Random(0xe91c12)
ROW = 8192

def rgb565(c):
    return ((c >> 8) & 0xf800) | ((c >> 5) & 0x07e0) | ((c >> 3) & 31)

def pack_lane(c):
    # Independent channel truncation, then model signed SSE2 packing bias.
    word = (((c >> 16) & 255) // 8) * 2048
    word += (((c >> 8) & 255) // 4) * 32 + (c & 255) // 8
    signed = word - 32768
    packed_signed = min(32767, max(-32768, signed)) & 65535
    return packed_signed ^ 32768

pixels = 0
for word in range(65536):
    color = ((word >> 11) << 19) | (((word >> 5) & 63) << 10) | ((word & 31) << 3)
    for high in (0, 0xff000000):
        assert rgb565(color | high) == pack_lane(color | high) == word
        pixels += 1
for _ in range(200000):
    color = rng.getrandbits(32)
    assert rgb565(color) == pack_lane(color)
    pixels += 1


def row(mem, src, dst, offset, width, bpp, fast):
    offset &= ROW - 1
    overlap = (dst - src < ROW * 4) if dst >= src else (src - dst < width * bpp)
    if not fast or overlap:
        for x in range(width):
            a = src + ((offset + x) & (ROW - 1)) * 4
            color = int.from_bytes(mem[a:a + 4], 'little')
            result = rgb565(color) if bpp == 2 else color
            mem[dst + x*bpp:dst + (x+1)*bpp] = result.to_bytes(bpp, 'little')
        return
    left = width
    while left:
        count = min(left, ROW - offset)
        start = src + offset * 4
        if bpp == 4:
            mem[dst:dst + count * 4] = mem[start:start + count * 4]
        else:
            x = 0
            while x + 8 <= count:
                values = [int.from_bytes(mem[start + (x+i)*4:start + (x+i+1)*4], 'little') for i in range(8)]
                mem[dst + x*2:dst + (x+8)*2] = b''.join(pack_lane(c).to_bytes(2, 'little') for c in values)
                x += 8
            while x < count:
                color = int.from_bytes(mem[start+x*4:start+(x+1)*4], 'little')
                mem[dst+x*2:dst+(x+1)*2] = rgb565(color).to_bytes(2, 'little')
                x += 1
        left -= count
        dst += count * bpp
        offset = 0

seed = rng.randbytes(160000)
cases = 0
for bpp in (2, 4):
    for width in (*range(18), 31, 32, 33, 239, 240, 241, 319, 320, 321, 8191, 8192, 8193, 16401):
        for offset in (0, 1, 7, 8185, 8191, 8192, 0xffffffff):
            for dst in (0, 124, 128, 132, 136, 40000):
                expected, actual = bytearray(seed), bytearray(seed)
                row(expected, 128, dst, offset, width, bpp, False)
                row(actual, 128, dst, offset, width, bpp, True)
                assert actual == expected, (bpp, width, offset, dst)
                cases += 1
print(f'PASS integer model: {pixels} color/packing cases, {cases} wrapped/tail/alias rows')
print('C++/SSE2, renderer integration, game replay and PS4 performance NOT tested; no compilation.')
