# PS4 V: callback-preserving SH3 timer batching

> Update: user authorized building; cumulative V passed compiled unit/replay/PKG checks. See [V validation](PS4_V_VALIDATION.md). Earlier source-only statuses below are historical; PS4 hardware results remain pending.

Baseline U: 9c230f51ee34dfb14ea6aa144513087ff6006fe2 (runtime built from e9ac4bef7).
User hardware feedback: generally above 50 FPS, complex scenes still slow with
audio stutter. A new host U profile still samples SH3 execution and scalar TMU
advancement. This profile is Linux evidence, not a PS4 hotspot measurement.

The original running timer advances one prescaled tick at a time. V combines
only the ticks strictly before the next callback into integer additions and
subtractions. The firing tick remains scalar; callback changes to prescaler,
reset/rearm/stop/retrigger and residual count are observed before proceeding.
No instruction timing, emulated CPU clock, timer ordering or blitter ordering
changes. Non-positive dividers and trigger/wrap boundaries retain scalar
behavior. A stopped timer first subtracts one divider tick, avoiding division
when that was the only tick available; larger residuals still use modulo.

This does not change audio samples, stretch audio, skip frames or reduce
bullets. Shorter emulation time may help real-time audio delivery, but no claim
is made that audio stutter is fixed. The S frontend remains unchanged.

## Validation

- ASan/UBSan differential against the original pre-S scalar timer: 320,024
  steps plus callback-state traces, including changing prescalers, reset,
  stop/rearm, unsigned wrap, zero trigger, null callback, negative divider and
  a zero divider repaired by its first callback. Legacy permanently stopped
  zero-divider states remain outside the test (the original does not finish).
- Serial U/V/V/U 3,000-frame runs: video/audio/final-state hashes match.
- DDPSDOJ mean core time 3.65660 -> 3.62185 ms (-0.95%); too small to establish
  a reliable improvement. DDPDFK 2.83090 -> 2.63445 ms (-6.94%). These are
  combined timer-change host measurements, not PS4 FPS.
- Both games' 2/3-thread 3,000-frame replays and 2,400-frame lifecycle runs
  match U; Mushisam lifecycle matches too. Source hashes and individual
  measurements are in tests/sh3_stopped_timer/verified-v-replays-20261004.json.

Local profile/build/package evidence is in project testbuild/v-logs. Keep U as
the comparison package. V hardware performance, audio continuity and console
stability require separate testing; no full-speed result is claimed.

The user also reported full console power-off, no error screen, and a possible
association with klog. Causality is unknown. The first reconnect was refused;
a later single connection was explicitly requested by the user and succeeded.
The filename V-klog-20261004-122859.log contains U/RAPS10018 gameplay, not V
hardware validation. A TCP receive timeout does not prove the PS4 is alive.
Do not turn capture errors into repeated automatic reconnect attempts.

## Latest user stop (2026-10-04)

The user said "先不编译" after the timer-only PS4 build had already completed.
Do not start any further compiler, linker or packager until that instruction
is lifted. No V PKG has been generated; U is still the installable baseline.
The timer-only binary is archived in project
`testbuild/v-logs/binaries/timer-only-before-stop`, with its actual build commit
and hashes in `testbuild/v-logs/CURRENT_STATE.json`.

A subsequent source change adds bounded core/CPU/final-wait peaks and counts
of core frames exceeding 16,667/20,000 microseconds. It uses the existing clock
reads, still emits only one report per 300 frames, and stops after 24 reports.
Synthetic ASan/UBSan checks for peaks, thresholds, reset and syscall count
passed before the stop. This diagnostic change is NOT in the PS4 binary.
Core duration excludes frontend/pacing time and is not an audio underrun count.
The reason for adding it is that 300-frame averages can conceal short stalls.

The new klog connection did capture U/RAPS10018 DDPDFK windows. The console
power-off association with klog remains unproven. Capture was reconnected once
only after the user's explicit "你现在重连 我运行游戏" instruction; no automatic
reconnect loop is authorized by that one-time request.

## CPU follow-up: exact reciprocal division (source only)

The user requested another CPU optimization round for frame rate, retaining
"先不编译". The U host sample contains 841/2883 core samples in Sh3Run;
timer work was one part of that loop, not the entire sampled cost. Previously
compiled timer batching still used variable integer division for multi-tick
runs. This follow-up replaces those quotient/remainder calculations on
x86-64 with a cached reciprocal multiply and one correction. It does not
change instruction dispatch, CPU clocks, timer granularity, tick accounting,
callback order or emulated divider remainder.

For unsigned 32-bit n and nonzero d, cache r = floor(2^32 / d). Then
q0 = floor(n*r / 2^32) is either floor(n/d) or one less. Because r does not
overestimate 2^32/d, q0*d <= n. Correct with `n-q0*d >= d`. The remainder
specialization subtracts d once if needed, avoiding a second multiply to
recover the remainder from the corrected quotient. Divisor 1 requires a
33-bit reciprocal, so the cache and product use UINT64; the product stays
strictly below 2^64 for every n in the uint32 domain.

Two derived fields are initialized during timer config. Each use checks the
current prescaler, so callbacks/direct restored field changes invalidate the
cache without hooks in save/load or setters. Cache fields are not scanned.
The existing call sites exclude divisor zero and preserve the original
zero-divider behavior; this is not a change to that edge case. Non-x86-64
platforms use the original native division/remainder, avoiding an unmeasured
64-bit multiply cost on PowerPC or other 32-bit CPUs.

Validation performed without compilation:

- `python3 -B tests/sh3_stopped_timer/static_reciprocal.py`: 1,650,438
  quotient/remainder comparisons and 100,000 callback-boundary skip checks.
  Covers reduced-domain exhaustive input, full-width boundaries, actual TMU
  divider sizes, signed-negative prescalers interpreted as unsigned, divisor
  changes/reuse and overflow bounds. This is an integer model, not C++ timer
  execution or complete callback-sequence validation.
- Existing C++ timer differential test was extended with 1,200,000 helper
  quotient/remainder cases; **not compiled or run**. Its earlier 320,024-step
  callback trace pass applies to c711071c6, not this follow-up.
- Source diff whitespace check passed. No new native/PS4 binary or PKG.

Pending: actual helper and callback-trace ASan/UBSan tests; U/timer-only/
reciprocal A/B replay including reset/load/2–3 threads; compiler assembly
inspection; host and PS4 dense-scene measurements. Cache comparison, memory
loads and changed struct layout can offset arithmetic savings, especially
when divisors change often or multi-tick operations are rare. Keep only if
measured improvement justifies it. The SSE2 candidates are also still
uncompiled; test these logical changes separately before combining results.
