#!/usr/bin/env python3
"""Pure integer proof checks; does not execute the C++ timer or a compiler."""
import random

LIMIT = 1 << 32
MASK = LIMIT - 1
rng = random.Random(0x5a43d1)


class Divider:
    def __init__(self):
        self.divisor = 0
        self.reciprocal = 0
        self.rebuilds = 0

    def divide(self, value, divisor, want_remainder=False):
        assert 0 <= value < LIMIT and 0 < divisor < LIMIT
        if self.divisor != divisor:
            self.reciprocal = LIMIT // divisor
            self.divisor = divisor
            self.rebuilds += 1
        product = value * self.reciprocal
        assert product < 1 << 64
        quotient = product >> 32
        assert quotient * divisor <= value
        remainder = value - quotient * divisor
        if want_remainder:
            return remainder - divisor if remainder >= divisor else remainder
        return quotient + (remainder >= divisor)


model = Divider()
checks = 0

def check(n, d):
    global checks
    quotient = model.divide(n, d)
    assert quotient == n // d, (n, d, quotient)
    rebuilds = model.rebuilds
    assert model.divide(n, d, want_remainder=True) == n % d
    assert model.rebuilds == rebuilds
    checks += 1


# Exhaust the reduced domain, with cached repeated queries per divisor.
for d in range(1, 257):
    for n in range(4096):
        check(n, d)

# Full-width boundary divisors, real TMU fixed-point dividers and adjacent
# values. This includes signed-negative prescalers cast to uint32 in the
# stopped-timer path, and divisor 1 whose reciprocal needs 33 bits.
divisors = {1, MASK, 100000, 400000, 1600000, 6400000, 25600000, 102400000}
for bit in range(32):
    for delta in (-1, 0, 1):
        d = (1 << bit) + delta
        if 0 < d < LIMIT:
            divisors.add(d)
for d in sorted(divisors):
    values = {0, 1, MASK, d - 1, d, min(MASK, d + 1)}
    for quotient in (1, 2, 3, 31, 1024, MASK // d):
        for delta in (-1, 0, 1):
            n = quotient * d + delta
            if 0 <= n < LIMIT:
                values.add(n)
    for n in values:
        check(n, d)

# Changing/restored divisor with reused cache, and callback-boundary skip.
for _ in range(100000):
    d = rng.randrange(1, LIMIT)
    n = rng.randrange(LIMIT)
    check(n, d)
    check(MASK, d)
    for replacement in (400000, d, 1, d):
        check(n, replacement)
    before_callback = rng.randrange(LIMIT)
    scalar_skip = min(n // d, before_callback)
    fast_skip = min(model.divide(n, d), before_callback)
    assert fast_skip == scalar_skip
    assert n - fast_skip * d == n - scalar_skip * d

print(f"PASS integer model: {checks} quotient/remainder cases; 100000 callback-boundary skip checks")
print("C++/sanitizer timer tests, replay equivalence and PS4 performance remain pending; no compilation.")
