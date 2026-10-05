# CV1000 query-first demand alpha cache

Parent d22990c7174d89cc3b544ca5f16702171182a555. The ordered CPU blitter owner keeps the existing128-page/2-way transparency cache and4 queryanswers perpage. Capacity, replacement order, worker scheduling, command/guest cycles and blend arithmetic are unchanged.

## Change and correctness

Each32x32 page tracks8 resident16x8 tiles with one UINT32. Page lookup initializes metadata only. Cached queryanswers return before any demand-fill checks; a newquery ensures every intersecting tile before computing bounds. The first wholepage query retains the original scan and complete-page queries skip further fill work. Partial-first pages initialize all row/group masks to zero, then OR newlyread tiles into them. Group masks may conservatively contain pixels outside aquery, but cannot omit any requestedpixel once ensure returns. Oldqueryanswers remain correct when other tiles load, because every coveredtile was alreadyresident when theiranswer wascomputed.

All VRAM writes continue toinvalidate affectedpage tags. Replacement/reset invalidates querykeys and clears loadedmetadata. State loads keep the existing fullcache invalidation. No persistentstate format, GPU path or system setting changes.

## Validation

Complete renderer ASan/UBSan and scalar suites pass, including33837 new brute-force transparency bounds comparisons covering arbitraryquery order, emptyanswers, tile/group/page edges, singlepixel edits, clear and eviction. PS4Clang14 compilation passes.48 game replays match complete video/audio and endstate hashes for DDPSDOJ,DDPDFK,Ibara. Four2400-frame lifecycle runs (alsoMushisama) match golden hashes through JIT toggles, reset, save/restore, AV masks and rendering-core changes. Formal-source PS4 renderer object is compared byte-for-byte with tested object before commit.

## Host measurements and limits

Both timing matrices use the same process-local CPU pool2/4/6/8 for baseline/candidate; no global affinity policy is changed. Ryzen5600/WSL host results are not PS4FPS. Main-thread CPU includes callbacks and excludes raster workers; wall and CPU deltas sometimes diverge, so report both.

JIT wall mean first/reverse: DDPSDOJ +0.42/-3.35%, DDPDFK -1.39/-2.11%, Ibara -0.97/+0.41%. P95 first/reverse: -1.38/-2.52%, -2.30/-2.54%, -0.87/-1.48%. Reverse main-threadCPU means -3.09/-1.93/+0.42%; same120 slowframesCPU DDPS -2.24%,DDP -1.75%. DDPS noJIT reversewall -5.71% butCPU only-0.13%; Ibara noJIT wall+1.56%/P95+2.78% whileCPU-1.09%. Do not cherry-pick wall gains or claim uniform acceleration.

Earlier8x8 and16x8 eager-demand checks were notretained; moving demand checks afterquery-cache lookup is part of this final change. Diagnostic8x8 estimates of25%fewerpixels are not measured speed or exact16x8 savings. No diagnostic code retained.

Evidence: project testbuild/cv1k-alpha-queryfirst-logs (REPORT,retained,summary,slow-window,repeat,dual,lifecycle), cv1k-alpha-demand-profile for granularity investigation.
