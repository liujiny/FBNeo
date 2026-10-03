# PS4 CV1000 sync from PS5 r20/r21

Source: liujiny/pemu `ps5-gpu-opengl`, commits
`2a2f2ef25ae2d38d3a779935ddb0fc21bafd9993` and
`f3dfe3230c5ac859cc795107ef317dfee645f110`.
PS4 baseline: P `5411a5dff84d409bb98e350190e200439a5e12f3`.

The P core already has Salvia exact fixed-alpha blending, RGB565 extraction,
and one ordered persistent POSIX blitter worker. PS5 r21 uses an equivalent
SDL worker. Retain the PS4 backend; replacing it with SDL adds no algorithm.

Port missing barriers before driver RAM reset, state scan, CPU teardown and
clip-margin changes. Skip identical margin writes to avoid a data race. Make
worker reinit drain old work, enforce ordered submissions and synchronous
start-failure fallback, and avoid mutex acquisition when no job is pending.
Keep the existing PS4 warmup/save layout and configurable 1/2/3 render cores.
Commands remain serial on one worker because VRAM uploads/draws depend on
previous commands. No CPU clock, emulated busy timer, IRQ or opcode changes.

PS5 native heap type correction, loader, SDL audio/input and pEMU state-file
buffers belong to different runtime/frontends. Do not port them to RetroArch.
PS5 periodic profiling was already absent from normal PS4 builds. PPC DRC and
Xenos GPU paths are not x86-64/Piglet optimizations.

Host worker tests cover 6000 jobs, main/worker overlap, startup warmup,
mutex/condition/thread creation failures, disable, scan/reset/reinit/exit
barriers and one-core fallback. Replay DDPDFK and DDPSDOJ against P, including
late coins/start, shooting/bombs and save/load/reset/thread changes. Detailed
results are in project `testbuild/q-logs`; host timing is not PS4 FPS.

P was accepted on PS4 for KOV2, Mushisam gameplay/sound, GA2 and core switching.
DDPDFK stutter motivated this audit. Q still requires console performance
comparison; the major PS5 acceleration was already present, so these safety
and synchronization changes do not promise full speed. Reported later-ROM
sound issue is explicitly deferred by the user and unchanged here.
