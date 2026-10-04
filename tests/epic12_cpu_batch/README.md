# EPIC12 CPU batch differential test

Status on 2026-10-04: **ASan/UBSan and scalar differential runs passed** after
the user authorized compilation. ThreadSanitizer compiled but could not start
on this host (`unexpected memory mapping`), including a non-PIE attempt. These
results do not establish PS4 performance. Logs are outside this repository at
`testbuild/x-ibara-logs`.

```sh
python3 tests/epic12_cpu_batch/run.py --sanitizer address,undefined --output /tmp/epic12-batch-asan
python3 tests/epic12_cpu_batch/run.py --sanitizer thread --output /tmp/epic12-batch-tsan
python3 tests/epic12_cpu_batch/run.py --scalar --output /tmp/epic12-batch-scalar
```

The test includes the production device implementation and compares its output
with the original generated rasterizers. It checks full VRAM and the emulated
blit-delay counter. Two 128 MiB VRAM buffers are allocated; allow extra memory
for sanitizers. Artifacts are kept outside the production object directories.

Coverage includes:

- Fixed-alpha and plain/tinted paths; unsupported blend fallback; flips, clipping,
  empty rectangles, wrapped sources, overlapping copies and SIMD tails.
- Read-after-write and write-after-read barriers, ordered writes to the same
  pixels, uploads between draws, clip changes, normal/unknown list terminators
  and the 256-command capacity boundary.
- Transparent pixels with nonzero RGB, cached empty queries becoming opaque,
  metadata invalidation after writes, restore-equivalent VRAM replacement and
  the actual device reset entry point.
- All 65,536 upload words; payload copies and upload rows against the original
  word readers; odd addresses, repeated RAM wrap, address overflow, output guards
  and upload timing accounting.
- One/two/three lanes, persistent helper reuse, simulated `pthread_create`
  failure, reinitialization, option changes and execution under the existing
  asynchronous list owner.

Native equality and sanitizer success do not establish PS4 performance or full
game correctness. W/X replay comparisons also matched video/audio/state for
Ibara, DDPSDOJ, DDPDFK and Mushisama, with lifecycle/thread changes and both
Ibara output formats. Test busy scenes on PS4 separately. The production
fallback can be built with `FBNEO_EPIC12_CPU_BATCH=0`.
