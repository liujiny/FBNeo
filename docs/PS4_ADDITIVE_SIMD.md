# PS4 additive blend candidate — source only

> Update: user authorized building; cumulative V passed compiled unit/replay/PKG checks. See [V validation](PS4_V_VALIDATION.md). Earlier source-only statuses below are historical; PS4 hardware results remain pending.

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
31/32), and destination alpha 31 or 0. Byte saturation and source bit 29 match
the scalar formula. Transparent lanes preserve the full destination word;
an entirely transparent block returns before destination access.

Shifted overlapping source/destination ranges within a row keep the scalar
loop. Identical ranges and disjoint ranges are eligible. Integer address
distance avoids comparing unrelated pointers or overflowing range ends.
Rows retain their original order, so cross-row aliasing is unchanged. Tails,
flipped rows, other tint/alpha modes and non-x86-64 platforms remain scalar.
Clipping, source-wrap rejection and emulated blitter delay are unchanged.

## Validation and remaining work

Pure Python integer modeling passed 286,720 pixel comparisons and 23,664
shared-memory row cases, exercising 88,576 four-pixel blocks. This is not
execution of the C++ or SSE2 code. The image test now has 7,048 cases, including
1,024 new overlap/tail cases and an x86-64 SIMD coverage assertion.

**Pending until compilation is authorized:** native ASan/UBSan image tests,
full-game U/candidate replay and state/audio/video equivalence, lifecycle and
2/3-thread cases, host performance comparison, PS4 build/object audit,
immutable S frontend/payload audit, PKG validation and hardware measurements.
Use the original U baseline as well as timer-only V to isolate this change.
Check sparse transparency/store bandwidth and code size; eligibility frequency
and any speedup are currently unknown. Retain only if measurement supports it.

## Second source-only round

The previous four-pixel candidate (5b627bcab) used packed 32-bit addition with
carry extraction, shifting and subtraction. The current helper uses unsigned
byte-saturating addition instead: each masked component is `8*c`, so
`min(255, 8*a + 8*b) & 248 == 8*min(31, a+b)`. Bytes are independent, and
the source transparency bit is restored separately. This reduces the
arithmetic dependency chain without changing 5-bit color rounding. Actual
instruction count and performance still need compiler and hardware checks.

The same guarded four-pixel helper now supports destination alpha zero.
Opaque blocks only load source and write masked source color/bit 29;
transparent blocks preserve inactive destination lanes and therefore still
read destination. Fully transparent blocks skip destination accesses.
Shifted overlapping rows remain scalar; no memcpy/memmove semantics replace
the original sequential emulated-VRAM behavior.

Both rounds are **uncompiled candidates**. Python results are algebra/order
checks only, not renderer or game validation. The last installable package
remains U. No new timer-only SELF, frontend, or package was created.
