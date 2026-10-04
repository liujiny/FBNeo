# EPIC12 output-row differential test

Build/run with `c++ -O2 -g -fsanitize=address,undefined -fno-omit-frame-pointer
tests/epic12_screen_copy/test.cpp -o /tmp/epic12-screen-test`, then run
`/tmp/epic12-screen-test`. Checks 2,184 rows against independent per-pixel
addressing/conversion, including guard bytes, wrap, tails and aliasing.
See ../../docs/PS4_V_VALIDATION.md for archived V results.
