// Diagnostic-only bounded sampling. CPU phase includes synchronous device
// handlers and waits inside SH3; worker execution may overlap that phase.
#if defined(__PS4__) || defined(CV1K_PROFILE_TEST)
#ifndef CV1K_PROFILE_TEST
#include <orbis/libkernel.h>
#endif
#include <stdio.h>
#include <string.h>
static struct {
 unsigned frames, windows, cpu_mask;
 unsigned long long last, previous_end, sum[5], peak;
 bool active;
} cvp;
static void cvp_reset() { memset(&cvp, 0, sizeof(cvp)); }
static void cvp_begin() {
 cvp.active = cvp.windows < 24;
 if (!cvp.active) return;
 cvp.last = sceKernelGetProcessTime();
 int cpu = sceKernelGetCurrentCpu();
 if (cpu >= 0 && cpu < 32) cvp.cpu_mask |= 1u << cpu;
 if (cvp.previous_end) cvp.sum[4] += cvp.last - cvp.previous_end;
}
static void cvp_phase(unsigned phase) {
 if (!cvp.active) return;
 unsigned long long now = sceKernelGetProcessTime();
 cvp.sum[phase] += now - cvp.last; cvp.last = now;
}
static void cvp_end(const char *game, unsigned dips, int cores) {
 if (!cvp.active) return;
 cvp.previous_end = cvp.last;
 if (++cvp.frames != 300) return;
 char line[512];
 snprintf(line, sizeof(line), "[PS4 CVPERF] game=%s window=%u frames=300 cpu_us=%llu audio_us=%llu sync_us=%llu draw_us=%llu outside_us=%llu render_cores=%d dips=%02x cpu_mask=%x\n",
 game, cvp.windows, cvp.sum[0]/300, cvp.sum[1]/300, cvp.sum[2]/300, cvp.sum[3]/300, cvp.sum[4]/300, cores, dips, cvp.cpu_mask);
 sceKernelDebugOutText(0, line);
 cvp.frames=0; cvp.cpu_mask=0; ++cvp.windows; memset(cvp.sum,0,sizeof(cvp.sum));
}
#else
static void cvp_reset() {}
static void cvp_begin() {}
static void cvp_phase(unsigned) {}
static void cvp_end(const char *, unsigned, int) {}
#endif
