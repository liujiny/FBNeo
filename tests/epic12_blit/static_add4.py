#!/usr/bin/env python3
"""Integer model only: does NOT compile or execute C++/SSE2 renderer code."""
import random

MASK = 0x00f8f8f8
VALID = 0x20000000
U32 = 0xffffffff
rng = random.Random(0x12add4)


def reference(s, d, transparent, add_destination):
    if transparent and not s & VALID:
        return d
    result = s & VALID
    for shift in (3, 11, 19):
        result |= min(31, ((s >> shift) & 31) + (((d >> shift) & 31) if add_destination else 0)) << shift
    return result


def lane(s, d, transparent, add_destination):
    # Model epi32 shift/sign expansion, packed carry, and bitwise selection.
    sign = ((s << 2) & U32) >> 31
    valid = U32 if sign else 0
    source, target = s & MASK, d & MASK
    result = 0
    for shift in (0, 8, 16, 24):
        byte = (source >> shift) & 255
        if add_destination:
            byte = min(255, byte + ((target >> shift) & 255))
        result |= byte << shift
    result = (result & MASK) | (s & VALID)
    return ((result & valid) | (d & (U32 ^ valid))) if transparent else result


def block(s, d, transparent, add_destination):
    if transparent and all(not v & VALID for v in s):
        return list(d)
    return [lane(a, b, transparent, add_destination) for a, b in zip(s, d)]


def draw(memory, src, dst, width, height, stride, transparent, vector, add_destination):
    blocks = 0
    for y in range(height):
        a, b = src + y * stride, dst + y * stride
        x = 0
        # Same byte-distance guard as the candidate; one row at a time.
        if vector and (a == b or abs(a - b) * 4 >= width * 4):
            while x + 4 <= width:
                source = memory[a + x:a + x + 4]
                destination = memory[b + x:b + x + 4]
                memory[b + x:b + x + 4] = block(source, destination, transparent, add_destination)
                blocks += 1
                x += 4
        while x < width:
            memory[b + x] = reference(memory[a + x], memory[b + x], transparent, add_destination)
            x += 1
    return blocks


checks = rows = blocks = 0
for add_destination in (False, True):
    for shift in (3, 11, 19):
        for a in range(32):
            for b in range(32):
                for flag in (0, VALID):
                    for transparent in (False, True):
                        s, d = (a << shift) | flag, b << shift
                        assert lane(s, d, transparent, add_destination) == reference(s, d, transparent, add_destination)
                        checks += 1

    # All transparency masks, arbitrary spare bits, independent pixel lanes.
    for case in range(16384):
        s = [(rng.getrandbits(32) & ~VALID) | (VALID if case & (1 << i) else 0)
             for i in range(4)]
        d = [rng.getrandbits(32) for _ in range(4)]
        for transparent in (False, True):
            assert block(s, d, transparent, add_destination) == [reference(a, b, transparent, add_destination) for a, b in zip(s, d)]
            checks += 4

    # Shifted overlap, equal/disjoint ranges, cross-row aliasing, zero widths,
    # unaligned addresses and every four-pixel tail length.
    for width in range(34):
        for delta in (*range(-40, 41), -129, -128, -127, 127, 128, 129):
            for height in (1, 3):
                for transparent in (False, True):
                    data = [rng.getrandbits(32) for _ in range(1024)]
                    expected, actual = list(data), list(data)
                    draw(expected, 256 + delta, 256, width, height, 128, transparent, False, add_destination)
                    blocks += draw(actual, 256 + delta, 256, width, height, 128, transparent, True, add_destination)
                    assert actual == expected, (add_destination, width, delta, height, transparent)
                    rows += 1
assert blocks > 0
print(f"PASS integer model: {checks} pixel checks, {rows} alias/row cases, {blocks} four-pixel blocks")
print("C++/SSE2 compilation, renderer differential tests, game replay and PS4 performance remain pending.")
