# EPIC12 CPU batch differential test

Status on 2026-10-04: written and reviewed, **not compiled or executed**. The user
requested source changes only. The commands below are for a later authorized
validation step; their presence is not a test result.

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
game correctness. Before packaging, run Ibara and other CV1000 games with the
same inputs under W baseline and the candidate, including video/audio/state,
reset/load/unload and 1/2/3-thread comparisons. Test the PS4 separately. The
production fallback can be built with `FBNEO_EPIC12_CPU_BATCH=0`.
