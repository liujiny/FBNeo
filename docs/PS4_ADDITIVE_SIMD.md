# PS4 additive blend candidate — source only

2026-10-04. User requested reading klog and another optimization round, while
retaining the latest instruction to stop before compilation. No build or PKG
was produced for this change. Parent 8d247f9b0 contains timer batching and
bounded timing diagnostics; the previously compiled timer-only c711071c6
SELF does not contain this candidate or the new diagnostics.

## Evidence and scope

The capture `testbuild/klog/V-klog-20261004-122859.log` records **U/RAPS10018**,
not V. After the early windows, DDPSDOJ window 7 has 18,079 us mean CPU/device
time over 300 frames. DDPDFK has up to 18,049 us per worker job and 3,651 us
mean final sync wait, in different windows. CPU time includes devices and
internal waits; worker time is per job rather than per frame. These figures
identify CPU/device and drawing pressure but do not measure this specific
blend's share or prove that this candidate will improve performance.

Core audio mixing is about 38–70 us in these later windows. No frontend audio
underrun counter was captured, so audio stutter is not assigned a proven cause.
User reports whole-console power loss; the log does not establish its cause
or an association with klog. Keep capture bounded and do not auto-reconnect.

## Change

On x86-64 with SSE2, the existing fixed-alpha helper processes four pixels at
a time only for forward rows, source alpha 31, identity tint (each channel
31/32), and destination alpha 31. Packed saturation and source bit 29 match
the scalar formula. Transparent lanes preserve the full destination word;
an entirely transparent block returns before destination access.

Shifted overlapping source/destination ranges within a row keep the scalar
loop. Identical ranges and disjoint ranges are eligible. Integer address
distance avoids comparing unrelated pointers or overflowing range ends.
Rows retain their original order, so cross-row aliasing is unchanged. Tails,
flipped rows, other tint/alpha modes and non-x86-64 platforms remain scalar.
Clipping, source-wrap rejection and emulated blitter delay are unchanged.

## Validation and remaining work

Pure Python integer modeling passed 143,360 pixel comparisons and 11,832
shared-memory row cases, exercising 44,288 four-pixel blocks. This is not
execution of the C++ or SSE2 code. The image test now has 6,536 cases, including
512 new overlap/tail cases and an x86-64 SIMD coverage assertion.

**Pending until compilation is authorized:** native ASan/UBSan image tests,
full-game U/candidate replay and state/audio/video equivalence, lifecycle and
2/3-thread cases, host performance comparison, PS4 build/object audit,
immutable S frontend/payload audit, PKG validation and hardware measurements.
Use the original U baseline as well as timer-only V to isolate this change.
Check sparse transparency/store bandwidth and code size; eligibility frequency
and any speedup are currently unknown. Retain only if measurement supports it.
