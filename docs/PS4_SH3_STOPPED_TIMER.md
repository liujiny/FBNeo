# SH3 stopped-timer prescaler optimization

R hardware sampling found DDPSDOJ/DDPDFK CPU/device phases exceeding the
16.67 ms frame budget in later gameplay windows, with the ordered blitter
worker successfully created. This alone does not isolate interpreter cost.
A separate Linux SIGPROF run of DDPSDOJ, with pulsed inputs entering gameplay,
found a hot loop repeatedly subtracting the prescaler of a stopped SH3 timer.
Host sample proportions and timing are not PS4 performance measurements.

When `running == 0`, `run_prescale` cannot invoke callbacks or change timer
state except the free-running prescale remainder. Reduce that remainder by
modulo instead of repeated subtraction. Running timers retain every tick,
callback order and opportunity for the callback to change the prescaler or
reset/rearm/stop the timer. No CPU clock, DIP, emulated delay, sound behavior
or frame-skipping policy is changed. The fast path also applies on the next
iteration if a callback stops a timer. No new persistent state is added.

`tests/sh3_stopped_timer/test.py` extracts production and R baseline timer
implementations and compares 160000 steps under ASan/UBSan, including counter
wrap and callbacks that reset, rearm, stop or change the divider. Gameplay
replay compares video/audio hashes and final states for DDPSDOJ/DDPDFK in
single-thread, threaded and save/load/reset/core-reload scenarios. Detailed
results are retained in project `testbuild/s-logs`.

PS4 frame-rate improvement remains pending hardware testing. R's bounded
performance diagnostics are retained for like-for-like scene comparison.
The separately reported missing-audio issue remains deferred by request.
