# R CV1000 timing

Q user: DDPSDOJ roughly 40+ FPS; DDPDFK somewhat better but still stutters.
The existing ordered worker and fixed-alpha optimization do not establish
full-speed emulation. R is diagnostic, retaining Q CPU/graphics semantics.

PS4 CVPERF averages microseconds across 300 emulated frames for CPU/device
execution, audio, explicit synchronization, final redraw and the gap outside
DrvFrame. CPU includes mapped device callbacks and waits inside SH3;
asynchronous blitter work overlaps CPU time. Do not sum it with unmeasured
worker execution or interpret low redraw time as cheap software sprites.
Outside time includes frontend work, pacing and possible menu pauses.

One CVWORKER line reports actual thread creation. Core count and DIP byte
are included in timing windows. At 24 reports (7200 emulated frames), all
profiling clock calls stop until another game load. Reset/save do not restart
the diagnostic budget and no profiler state is added to guest save states.
Host tests cover 300-frame reporting, the exact cap, no clocks after the cap
and resetting on game load. PS4 stage timings remain pending.

Suggested reproduction: shader disabled, DDPSDOJ then DDPDFK for about a
minute each. Optionally compare render cores 2 and 1 in the same scene, within
the collection window. Apply Lottes last because it may still crash. Audio
bug work remains deferred at the user's request.
