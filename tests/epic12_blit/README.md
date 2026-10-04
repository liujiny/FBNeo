# EPIC12 fixed-alpha blend tests

Run with the same 32-bit big-endian PowerPC compiler and QEMU used by the SH3
backend tests:

```sh
python3 tests/epic12_blit/run.py --toolchain-root /path/to/toolchain/root --output /tmp/epic12-test
```

The test checks every tint/alpha/source-component combination (65,536 cases),
one million packed RGB saturated additions, and 6,536 complete image operations
against the original EPIC12 functions. Image cases cover both axes of flipping,
transparent and opaque modes, all alpha/tint ranges, clipping, source wrapping,
overlapping source/destination VRAM and the accumulated emulated blitter delay.
The first 1,024 image cases explicitly cover identity tint triplets (31/32),
source alpha 31 versus 30, adjacent tint 33, both axes of flipping, transparency
and destination alpha 0/15/30/31. These extensions passed the compiled native differential test for U.

When compilation must remain stopped, `python3 -B tests/epic12_blit/static_identity.py`
checks all 65,536 component rounding combinations and 262,144 packed RGB
identity cases using integer algebra. It does not execute the C++ renderer or
replace the compiled image differential test.

No ROMs are required. Synthetic device state and the original renderer are local
to this executable; unused emulator dependencies are removed by the linker.
`tests/sh3_ppc/tchar.h` supplies the minimal portability definitions.

The separate full-game regression uses private ROMs and checks video, audio and
complete final state. QEMU timings do not measure Xbox 360 frame rates.

## Pending SSE2 additive candidate (2026-10-04)

The first 6,024 cases passed on U. Another 512 image cases now cover widths
1..16, unaligned starts, scalar tails, identical/disjoint/shifted-overlapping
VRAM and consecutive rows. The x86-64 test requires the SSE2 path to execute.
These new C++ cases have NOT been compiled or run: the user requested no more
compilation. PowerPC retains the scalar implementation.

`python3 -B tests/epic12_blit/static_add4.py` runs only an integer model, without
compiler invocation. It passed 143,360 pixel comparisons and 11,832 row/alias
cases (44,288 modeled four-pixel blocks). This checks packed saturation, all
transparency masks, spare bits, tails and shared-memory ordering; it cannot
validate intrinsic lowering, actual C++ code, memory sanitizer behavior or speed.
