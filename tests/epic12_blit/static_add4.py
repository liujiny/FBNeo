#!/usr/bin/env python3
"""Integer model only: does NOT compile or execute C++/SSE2 renderer code."""
import random

MASK = 0x00f8f8f8
VALID = 0x20000000
U32 = 0xffffffff
rng = random.Random(0x12add4)


def reference(s, d, transparent):
    if transparent and not s & VALID:
        return d
    result = s & VALID
    for shift in (3, 11, 19):
        result |= min(31, ((s >> shift) & 31) + ((d >> shift) & 31)) << shift
    return result


def lane(s, d, transparent):
    # Model epi32 shift/sign expansion, packed carry, and bitwise selection.
    sign = ((s << 2) & U32) >> 31
    valid = U32 if sign else 0
    summed = (s & MASK) + (d & MASK)
    carry = summed & 0x01010100
    result = ((summed | ((carry - (carry >> 5)) & U32)) & MASK) | (s & VALID)
    return ((result & valid) | (d & (U32 ^ valid))) if transparent else result


def block(s, d, transparent):
    if transparent and all(not v & VALID for v in s):
        return list(d)
    return [lane(a, b, transparent) for a, b in zip(s, d)]


def draw(memory, src, dst, width, height, stride, transparent, vector):
    blocks = 0
    for y in range(height):
        a, b = src + y * stride, dst + y * stride
        x = 0
        # Same byte-distance guard as the candidate; one row at a time.
        if vector and (a == b or abs(a - b) * 4 >= width * 4):
            while x + 4 <= width:
                source = memory[a + x:a + x + 4]
                destination = memory[b + x:b + x + 4]
                memory[b + x:b + x + 4] = block(source, destination, transparent)
                blocks += 1
                x += 4
        while x < width:
            memory[b + x] = reference(memory[a + x], memory[b + x], transparent)
            x += 1
    return blocks


checks = 0
for shift in (3, 11, 19):
    for a in range(32):
        for b in range(32):
            for flag in (0, VALID):
                for transparent in (False, True):
                    s, d = (a << shift) | flag, b << shift
                    assert lane(s, d, transparent) == reference(s, d, transparent)
                    checks += 1

# Every transparency mask, arbitrary spare bits, and independent pixel lanes.
for case in range(16384):
    s = [(rng.getrandbits(32) & ~VALID) | (VALID if case & (1 << i) else 0)
         for i in range(4)]
    d = [rng.getrandbits(32) for _ in range(4)]
    for transparent in (False, True):
        assert block(s, d, transparent) == [reference(a, b, transparent) for a, b in zip(s, d)]
        checks += 4

rows = blocks = 0
# Include shifted overlap, identical ranges, disjoint rows, cross-row aliasing,
# zero widths, unaligned addresses and all 4-pixel tail lengths.
for width in range(34):
    for delta in (*range(-40, 41), -129, -128, -127, 127, 128, 129):
        for height in (1, 3):
            for transparent in (False, True):
                data = [rng.getrandbits(32) for _ in range(1024)]
                expected, actual = list(data), list(data)
                draw(expected, 256 + delta, 256, width, height, 128, transparent, False)
                blocks += draw(actual, 256 + delta, 256, width, height, 128, transparent, True)
                assert actual == expected, (width, delta, height, transparent)
                rows += 1
assert blocks > 0
print(f"PASS integer model: {checks} pixel checks, {rows} alias/row cases, {blocks} four-pixel blocks")
print("C++/SSE2 compilation, renderer differential tests, game replay and PS4 performance remain pending.")
