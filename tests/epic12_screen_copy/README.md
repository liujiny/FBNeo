# EPIC12 output-row conversion checks

While compilation is stopped, run only:

```
python3 -B tests/epic12_screen_copy/static_model.py
```

Pure integer/byte-array modeling passed 331,072 color/packing comparisons and
2,604 wrapped/tail/alias row comparisons. It covers every RGB565 output word,
arbitrary source high bits, zero/small/non-multiple widths, row wrap, widths
larger than a VRAM row, identical/shifted output overlap, and destination guard
bytes. It does not execute C++ or SSE2 and does not measure performance.

`test.cpp` is a standalone C++ differential test with 2,184 output rows and a
SIMD-path coverage assertion. It has NOT been compiled/run. When compilation
is authorized, build it with address/undefined-behavior sanitizers and run it,
then validate complete-frame integration and full-game audio/video/state
replays. Reference uses independent per-pixel conversion/addressing and checks
the whole memory buffer, including guard bytes and aliasing.

Example for later, **not during the current stop**:

```
c++ -O2 -g -fsanitize=address,undefined -fno-omit-frame-pointer tests/epic12_screen_copy/test.cpp -o /tmp/epic12-screen-test
/tmp/epic12-screen-test
```
