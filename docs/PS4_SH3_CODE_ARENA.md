# The SH3 code arena must outgrow the live working set

## What the arena is

`Sh3X64` compiles SH3 integer blocks into SysV AMD64 leaf functions inside one
anonymous mapping (`src/cpu/sh4/sh3_x64_jit.h`). `CODE_BYTES` is the size of
that mapping; `used` is a bump pointer, and each compiled block occupies
`(size + 15) & ~15` bytes at `code + used`.

Because the allocator never reuses dead space (a block that is replaced by a
cache collision keeps its bytes forever), the arena eventually fills. When it
does, `compile()` does the only thing it can do cheaply:

```c
if (used + SLOT_BYTES > CODE_BYTES) { memset(blocks, 0, SLOTS*sizeof(Block)); used = 0; }
```

That single line clears *every* `Block` record, so the whole working set is
recompiled inside the frame that hits the wrap. The block table (`SLOTS`,
`WAYS`, `CACHE_SETS`, `SLOT_BYTES`) is independent of `CODE_BYTES`, so only the
anonymous mapping grows; the table stays at 32768 slots (~2.9 MiB) regardless.

## History

- 4 MiB -> 8 MiB (parent `dabc93383df780a0c271d48d0b57be5175bb4a69`, commit
  `27476d177`): the 600-frame DDPSDOJ replay used to wrap in zero-based frames
  240-269, building 1940 native blocks vs 171 in the preceding 30 frames. The
  same window ran 36% slower with JIT than with the interpreter. At 8 MiB the
  window built 166 blocks and never wrapped; DDPS mean/P95 improved by
  -7.03/-12.10% (reversed repeat -6.55/-13.07%).
- 8 MiB -> 64 MiB (this change): 8 MiB was measured with only ~600-frame runs
  and was sized from a 5.4 MiB end state. Longer runs show that CV1000 titles
  keep emitting code long after the first 600 frames, so 8 MiB cannot hold the
  live working set and the wrap returns as a periodic stall.

## Why 8 MiB is not enough

Wrap counter instrumented on the *same* 20000-frame `ddpdfk.state` replay
(diagnostic builds, measurement only):

| arena | wraps in 20000 frames | wrap period | worst frame | final `used` |
|-------|----------------------|-------------|-------------|--------------|
| 8 MiB | 12 | ~1700 frames (~28 s) | 18.0-26.3 ms | 9.1 MiB (after wrap) |
| 64 MiB | 0 | - | 7.7 ms | 40.6 MiB |

Each 8 MiB wrap is one dropped frame at 60 Hz (budget 16.67 ms); the wraps are
not evenly spaced (measured gaps 356-5717 frames) because the emission rate
follows the scene. Emission is ~2 kB of code per frame, so 64 MiB covers
roughly 33000 frames (~9 minutes) of sustained CV1000 play before the next
wrap, versus ~1700 frames for 8 MiB.

Only cells large, dense scenes keep compiling: the state used for this round
still emits 40.6 MiB over 20000 frames, so this is a "wrap is now 20x further
away and no longer once every 28 seconds" claim, not a claim that the arena
never wraps.

## Measured effect (host execution of the PS4 CPU object)

Same state (`ddpdfk.state`, SHA256 of the two gold artifacts below), reverse
speculation order, `FBNEO_REPLAY_SH3_JIT=enabled`:

| frames | core | mean ms | p95 ms | max ms |
|--------|------|---------|--------|--------|
| 600 | 8 MiB | 2.2883 | 3.29 | 17.3-18.9 |
| 600 | 64 MiB | 2.2495 | 3.28 | 6.8-7.3 |
| 2400 | 8 MiB | 1.3517 | 2.9283 | 17.54 |
| 2400 | 64 MiB | 1.3355 | 2.9038 | 8.88 |

In the 8 MiB runs the >15 ms spike is deterministic and appears in frames
381-383 of every replay; in the 64 MiB runs it is gone, and the largest frame
is the first-frame warm-up. ddpsdoj also improves (mean 1.4655/1.4742 ->
1.4471/1.4485), ibara is flat within noise (1.5526 -> 1.5446 over 8 alternating
runs).

## What was checked and rejected in the same round

- `WAYS` 4 -> 8, a multiplicative set index, and both together: all slower than
  64 MiB alone (2.1502 / 2.1567 / 2.2898 vs 2.1090 at 600 frames). Rejected.
- Preferring an empty cache slot over `ROBIN` replacement on a miss: evictions
  went from 82k to 2311450 over 40000 frames, `used` from 9.1 to 43.2 MiB.
  Rejected.
- Never rewriting the arena, or compacting live blocks at wrap time: emitted
  code embeds absolute immediates, so blocks are not relocatable. Rejected.

## Validation

- `tests/sh3_x64/run.py`, including the 64 MiB arena boundary/recycle/
  stale-entry/edit/release case, passes with and without `--sanitize`
  (Clang ASan/UBSan).
- Gold video/audio/end-state hashes unchanged for `ddpdfk.state` (600 frames),
  `ddpsdoj.state` (600 frames) and `ibara.state6` (180 frames); 2400-frame
  `ddpdfk.state` runs are hash-identical between the 8 MiB and 64 MiB cores.
- `ps4-arena64m.o` (clang, `--target=x86_64-pc-freebsd12-elf`) differs from the
  same source compiled with 8 MiB *only* in the arena-size immediates
  (`0x800000` -> `0x4000000`, plus derived range masks). Relinked into a host
  core it reproduces the gold hashes and the same timing profile.
- 2400-frame toggle/reset/checkpoint/restore lifecycle matches the reference Y
  hashes for ibara and ddpsdoj.
- `CODE_BYTES` lives in `sh3_x64_jit.h`, which is only included under
  `#if FBNEO_SH3_X64_JIT`, so the interpreter-only build is byte-identical.

Evidence: `testbuild/sh3-arena64m-logs/` (build logs, PS4 object, dispatcher
diff, lifecycle results, `REPORT.md`), `testbuild/ah-ddpdfk-tail-logs/`
(A/B runs and wrap counters).

## Limits

This is Linux host execution of the PS4 CPU object with a native rest-of-core,
not PS4/Jaguar emulation and not measured hardware FPS. The 64 MiB mapping is
anonymous, so on Linux only touched pages are resident; PS4 physical commitment
of the larger mapping has not been measured on hardware, and the previously
shipped 8 MiB mapping was verified there. The JIT is still disabled by default.
No SELF/PKG/push/klog was performed as part of collecting this evidence.
