#!/usr/bin/env python3
"""Prove the identity-source integer reduction without compiling C++.

This algebra check does not execute the C++ renderer. The image differential
test and full-game replays must still run after compilation is authorized.
"""
import json
from pathlib import Path
import re


def main():
    source = (Path(__file__).resolve().parents[2] /
              'src/burn/devices/epic12_fast_blit.h').read_text()
    match = re.search(r'if \(IdentitySource\) s = pen & (0x[0-9a-f]+);', source)
    assert match, 'Identity path changed; review algebra check'
    mask = int(match.group(1), 16)
    assert mask == sum(31 << shift for shift in (3, 11, 19))
    identity_pairs = []
    for tint in range(64):
        for alpha in range(32):
            identity = True
            for component in range(32):
                original = min(31, min(31, component * tint // 31) * alpha // 31)
                identity &= original == component
            if identity:
                identity_pairs.append((tint, alpha))
    assert identity_pairs == [(31, 31), (32, 31)], identity_pairs
    # All RGB combinations for all eight identity tint triplets.
    cases = 0
    for tr in (31, 32):
        for tg in (31, 32):
            for tb in (31, 32):
                for rgb in range(32768):
                    r, g, b = rgb >> 10, (rgb >> 5) & 31, rgb & 31
                    pen = (r << 19) | (g << 11) | (b << 3)
                    original = (min(31, r * tr // 31) << 19) | (min(31, g * tg // 31) << 11) | (min(31, b * tb // 31) << 3)
                    # Include transparency and all unrelated bits: neither
                    # may leak into the packed colour sum.
                    assert pen & mask == original
                    assert (pen | (~mask & 0xffffffff)) & mask == original
                    cases += 1
    print(json.dumps({'compiled': False, 'component_rounding_cases': 65536,
                      'identity_tint_alpha_pairs': identity_pairs,
                      'rgb_identity_cases': cases,
                      'cpp_renderer_executed': False}, sort_keys=True))


if __name__ == '__main__':
    main()
