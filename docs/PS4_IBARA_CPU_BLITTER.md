# Ibara / CV1000 CPU drawing candidate

2026-10-04: X has been built after the user authorized compilation. Branch
`ps4-ibara-cpu-blitter-20261004`, based on W cleanup commit
`e32e95c1b1c463a9d72704125a6e4be0186869d9`; core build source is
`035759c35fc0e503ff42933c374b78b7978965d4` (initial candidate `9f952ac62`,
followed by an OpenOrbis macro-name fix and standalone test fixes). Existing
W/V packages remain available. PS4 gameplay/performance testing is pending.

Package: `RetroArch_PS4_X_Ibara_CPUBlit_RAPS10021.pkg`, Title ID `RAPS10021`,
Content ID `UP0001-RAPS10021_00-0000000000000001`, in the fixed project `out`
directory. It reuses the exact W frontend, other two cores and CRT shaders.
Build and validation evidence is in `testbuild/x-ibara-logs` outside this Git
repository; the output directory also contains `X_build_report.json`.

## Changes

- Cache the exact opaque bounds of 32×32 VRAM pages (64 sets, two ways), using
  the existing transparency bit. Four rectangle-query results per page avoid
  repeated edge scans. Writes invalidate resident pages; reset and state scan
  clear all derived metadata. Fully transparent sprites contribute the same
  emulated blit delay but require no pixel drawing.
- Batch up to 256 plain/tinted or fixed-alpha draws. Keep read-after-write and
  write-after-read barriers because sources remain in live CPU VRAM. Commands
  that write the same destination execute in their original order per row.
  Self-overlap, source wrapping and other blend modes use the existing routines
  after pending work has completed.
- Partition large batches into disjoint destination row ranges, weighted by
  accumulated pixel work. The ordered list owner draws one range, with at most
  two persistent helpers. The existing 1/2/3 software-rendering option selects
  the drawing lane count; SH3 emulation runs separately when threaded blitting
  is enabled. Small batches remain serial (16,384 pixels per requested lane).
- Use SSE2 for forward/flipped raw or identity fixed-alpha drawing, preserving
  scalar tails and transparency. Opaque forward copies use `memcpy` after
  dependency checks establish nonoverlap. Keep the original integer tint and
  alpha rounding; no floating-point colour conversion or frameskip is added.
- Copy contiguous upload payloads with wrap-aware `memcpy` and expand eight
  tRGB555 words at a time with SSE2. Preserve native word layout, odd address
  semantics, RAM wrapping, upload timing and output pixel representation.

The default is enabled only for PS4 and `FBNEO_RENDER_THREADS_TEST` builds.
Defining `FBNEO_EPIC12_CPU_BATCH=0` retains the W drawing/upload path for later
comparison. Other platform defaults are unchanged.

## Ordering and lifetime

The ordered blitter worker retains device state and command order. Helpers
never modify source pixels, cache metadata, command buffers or delay counters;
only their assigned output rows are writable. All helpers finish before an
upload, unsupported draw, dependency boundary or list terminator. Device init,
reset, scan and exit wait before replacing, restoring or releasing state.
Thread creation failure uses synchronous drawing. The existing 180-frame
synchronous startup period and Thread Blitter Off behavior remain in effect.
No affinity, priority, kernel or PS4 system-setting changes are involved.

## Source and evidence

Transparency metadata borrows the algorithm from Salvia's
`libretro/FBNeo/src/burn/devices/epic12_gpu_alpha.h`, inspected at commit
`c4e92f3380ac85ef1ee09897806806790ace8b9a` (file history:
`faf87bcf773a61fad927d7c79346f003a728c74b` and
`28e01aae08314cdd82ad01aecffd074cb3e1dc8b`). The PS4 implementation uses a
smaller CPU cache and SSE2, without the Xbox atlas, VMX or GPU interfaces.

The 2026-10-04 Ibara klog is from **V / RAPS10019**, not this candidate. It
contains 24 windows of 300 frames. CPU/device phase averages reach 19.337 ms
(window 9) and 19.038 ms (window 20), while final blitter waits in those windows
average 2 microseconds. That phase includes SH3, device callbacks, list
preparation and any waits inside them; it does not isolate interpreter cost.
There are also occasional long final waits. The capture includes different
scenes and option changes, so it is not a controlled thread-count comparison.
More drawing workers alone cannot be assumed to solve all frame overruns.

Local evidence is retained outside Git in
`testbuild/klog/cheat-crash-klog-20261004-144508.log`. Cheat-crash diagnosis was
paused at the user's request and introduces no changes in this candidate.

## Validation and remaining hardware checks

The [native differential suite](../tests/epic12_cpu_batch/README.md) passed with
ASan/UBSan and with the explicit SSE2 paths disabled. It uses the original
generated rasterizers as the pixel/timing oracle and includes parser, cache
and worker-lifetime cases. ThreadSanitizer compiled but could not start on
this host (`unexpected memory mapping`), including a non-PIE attempt; no
successful race-detector run is claimed.

Six pairs of 2,400-frame W/X native replays matched video/audio/state: Ibara,
DDPSDOJ, DDPDFK and Mushisama at 16-bit output with reset/load and thread-option
switching, Ibara at 32-bit output with the same lifecycle, and an uninterrupted
Ibara run. These are correctness checks, not evidence of PS4 speedup.

The PS4 dynamic SELF build passed. All 1,140 project objects are FreeBSD
x86-64, the ELF module header and essential libretro/thread APIs match W,
and the fSELF magic is valid. The unchanged W frontend passed the ELF/OELF/SELF
internal-heap and CRT-pointer audit. `pkg_validate` passed 32 checks; extraction
verified 18 payload hashes and R4 SFO semantics.

Next, measure busy scenes on PS4 with Thread Blitter enabled and identical
inputs at 2/3 drawing threads. Check cache miss overhead and row scheduling
as well as average/peak frame time. No PS4 speedup or full-frame-rate claim is
made yet; GPU offload remains a separate future path.
