# Ibara death-bomb raster optimization

## Actual PS4 reproduction

The user reports that AB still loses frames during the 1P death/bomb effect.
They supplied `ibara.state`, `ibara.state1` and `ibara.state6`; the first two
need no input and reach nearby explosion scenes after a short wait. `state6`
loads directly into a large explosion. These states replace generic gameplay
as the primary workload for this regression. No ROMs or states belong in Git.

Local evidence is in `testbuild/ibara-deathbomb-logs` relative to the project
root. `state-manifest.json` records original and extracted-core SHA256 values.
The states use RZIP v2 (Zstandard chunks), wrapping RASTATE v1; the MEM block
is the libretro state. Extracted states are 143391580 bytes. The reader checks
container bounds and decoded length; original files in the fixed out directory
are preserved. `extract-state.py` records the local extraction procedure.

## Observed bottleneck and change

Host-only phase timing of AB shows approximately 1.3–1.8 ms in CPU/device work
and 0.45–0.8 ms waiting for the blitter during sustained state6 explosions.
This is not a measurement of PS4 GPU time. The earlier states also contain
CPU-heavy bursts; one optimization need not solve both workloads.

A host CPU-time sampler on the ordered blitter thread finds 817 samples in
legacy s0/d4, 342 in tinted s0/d1 and 546 in the existing batch kernel/flush.
The sample counts are evidence of hotspots, not percentages of frame time.
The helper threads are not included in that sampler. Diagnostics and sampling
exist only in isolated native builds and have been removed from the candidate.

The candidate keeps AB's JIT and Y's row scheduler, with two targeted changes:

- Normalize constant source/destination factors 3/4/7 into fixed alpha, so
  inverse-alpha explosion draws can use the existing dependency-checked batch.
- Add four-pixel SSE2 for partial alpha/tint and destination factor 1 (tinted
  source). Mode selection is outside the pixel loop. Tint clamp, source-alpha
  rounding, destination multiplication, saturating add and transparency markers
  retain original integer semantics. Scalar tails use the original lookup tables.

The vector helper is taken from AA commit
`c605183cb0e3ee3a5fd58792385446a65f19d7ca`; AA's tile scheduler and broad blend-mode
batching are not adopted. Self-overlap and unsupported factors still fall back
to the original rasterizer. RAW/WAR barriers, WAW order, helper count, timing,
alpha-cache invalidation and lifecycle joins are unchanged. There is no new GPU
backend, worker count increase, clock setting or PS4 system change.

## Reproduction and scope

`compare.py` in the evidence directory runs AB/candidate/candidate/AB,
sequentially, with fixed time, no input, 16-bit video and three rendering lanes.
The full libretro `retro_run` interval is measured excluding hash callbacks.
Each state6 run is 180 frames; the earlier states are 600 frames each. Frame 0
is reported separately because first-use/JIT costs are variable, not discarded
silently. Frames 1..179 include the initial burst; 30..179 additionally isolates
the sustained explosion. Do not discard the first 180 frames of these fixtures.

All per-frame video/audio hashes and final serialized states must match AB.
`final-comparison.json` is the final timing set with no concurrent build/test
jobs. Earlier constant/source-factor experiments remain as intermediate evidence.
`validation-report.json` contains aggregate results and artifact hashes.

Renderer tests:

```sh
python3 tests/epic12_cpu_batch/run.py --sanitizer address,undefined --output <outside-tree>/simd
python3 tests/epic12_cpu_batch/run.py --scalar --sanitizer address,undefined --output <outside-tree>/scalar
```

Both cover the generated legacy rasterizer, all newly supported factor pairs,
partial alphas, saturated tint, flips, transparency, vector tails, multi-lane
overdraw, dependencies, upload timing and lifecycle/failure fallback. Five
2400-frame lifecycle replays (Ibara 16/32, DDPSDOJ, DDPDFK and Mushisam) compare
video/audio and final state with established Y references. Lifecycle checks
are correctness tests, not sustained-gameplay performance measurements.

Only native Linux verification has been performed for this candidate. No new
PS4 SELF/PKG has been built, no GitHub push or klog connection was made. The
current installable package remains AB. PS4 frame rate and worst-case spikes
still require the same actual save-state comparison on hardware.

## Final host results

Each mean below averages two runs, excluding only frame 0.

| Fixture | AB mean ms | Candidate mean ms | Time reduction |
| --- | ---: | ---: | ---: |
| ibara.state6 | 2.2513 | 1.6364 | 27.31% |
| ibara.state | 1.9019 | 1.8249 | 4.05% |
| ibara.state1 | 1.8202 | 1.7097 | 6.07% |

The sustained state6 interval (frames 30..179) changes from 2.1366 to 1.4592 ms (31.70% less time). Its mean per-run P95 changes from 3.2012 to 3.0970 ms (3.26% less). P95 in the earlier states is essentially unchanged. This improves sustained rendering load; it does not establish elimination of the worst explosion spikes. Frame-0 costs range widely (15.6–65.0 ms in state6) and are not claimed as a startup improvement. These are host timings, not PS4 FPS.
