# CV1000 video output optimization — uncompiled candidate

2026-10-04. User asked for another optimization round and explicitly broadened
inspection beyond CPU to GPU and other parts of the pipeline. The no-build
instruction remains active. No frontend, binary or package was changed.

## Pipeline inspection

U host profiling attributed 87/2883 core samples to epic12_draw_screen.
Hardware CVPERF draw_us is a core output stage (roughly 183 us in later U
DDPSDOJ windows), not a GPU/shader timing. The separately measured CPU/device
and blitter worker stages remain much larger; this candidate targets a smaller
recurring cost and cannot alone justify a stable-full-speed claim.

The preserved S frontend's gl2.c software upload path already uses a four-
texture rotation. GLES can upload directly when frame pitch is tightly packed;
padded frames and the 32-bit BGRA fallback can require conversion/copying.
The glFinish calls examined in hardware-render ring capture/presentation are
not evidence of unconditional stalls for this software core. Orbis swap
interval currently requests zero. These are source findings, not confirmation
of runtime GPU cost, selected pixel format, driver stalls, or shader load.
No synchronization removal or GPU API changes were made. For a later GPU
round, measure upload, draw/filter and swap separately in the actual PS4 path
before choosing a change; preserve multi-pass/PREV shader semantics.

## Source change

RGB565 output now branches on the confirmed output format once per row,
splits the source at the 8192-pixel VRAM wrap, and converts eight pixels per
iteration on x86-64 SSE2. The scalar conversion formula is unchanged, including
the original green-bit truncation. SSE2 signed packing biases 0..65535 into
-32768..32767 first and restores the high bit after packing. Tails stay scalar;
other architectures use the scalar contiguous loop. RGB1555/palette and
24-bit paths are unchanged. The core option controlling color depth is not
changed, and we do not assume the user's selected format from its default.

32-bit output uses memcpy for each continuous source span, preserving all
32 bits. Typical widths need one span, or two when crossing the VRAM edge.
Unlike the previous fixed 16-pixel unroll, the helper writes exactly the
requested width even for a non-multiple tail. Supported game widths retain
the original pixel sequence. The helper also handles multiple row wraps.

Both paths detect potential output/source-row overlap and preserve sequential
per-pixel reads/writes there. Row order, vertical scrolling, framebuffer size,
video format, GPU upload size, emulated blitter delay and the existing worker
completion wait remain unchanged. No direct GPU rendering/offload is claimed.

## Validation status

Pure Python model: 331,072 color/packing and 2,604 wrapped/tail/alias row cases
passed. New standalone C++ test covers 2,184 rows with a SIMD coverage check;
it is written but **not compiled or executed**. Existing U/game replay results
do not validate this code. Compilation, sanitizer checks, full-frame/replay
comparisons across both output formats and PS4 timings are pending. Test this
candidate separately from SIMD blending and reciprocal timers to attribute
any gain or regression. Libc memcpy overhead for short spans and per-row setup
may offset savings; keep based on measurement, not instruction-count claims.
