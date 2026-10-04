# EPIC12 fixed-alpha blend tests

Run with the same 32-bit big-endian PowerPC compiler and QEMU used by the SH3
backend tests:

```sh
python3 tests/epic12_blit/run.py --toolchain-root /path/to/toolchain/root --output /tmp/epic12-test
```

The test checks every tint/alpha/source-component combination (65,536 cases),
one million packed RGB saturated additions, and 6,024 complete image operations
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
