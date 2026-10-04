# EPIC12 blend differential tests

Run `python3 tests/epic12_blit/run.py --host --output /tmp/epic12-test`.
For PowerPC use `--toolchain-root /path/to/toolchain/root` instead of `--host`.
Checks 65,536 rounding cases, one million saturated additions and 7,048 image
operations against the original renderer, including transparency, flips,
clipping, wrap, aliasing, SIMD tails and emulated delay. No ROMs required.
See ../../docs/PS4_V_VALIDATION.md for the archived V validation result.
