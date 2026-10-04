// Bounded worker-side wall-time and CPU-placement observations. These are
// ordered blit jobs, not frame times; scheduling can affect their duration.
#if defined(__PS4__) || defined(EPIC12_PROFILE_TEST)
#ifndef EPIC12_PROFILE_TEST
#include <orbis/libkernel.h>
#endif
#include <stdio.h>
#include <string.h>
static struct {
 unsigned jobs, cpu_mask;
 unsigned long long begin, sum;
} cvw;
// Called only after the previous worker has been joined.
static void cvw_reset() { memset(&cvw, 0, sizeof(cvw)); }
// Remaining fields are accessed exclusively by the blitter worker.
static void cvw_begin() {
 if (cvw.jobs >= 7200) return;
 cvw.begin = sceKernelGetProcessTime();
 int cpu = sceKernelGetCurrentCpu();
 if (cpu >= 0 && cpu < 32) cvw.cpu_mask |= 1u << cpu;
}
static void cvw_end() {
 if (cvw.jobs >= 7200) return;
 cvw.sum += sceKernelGetProcessTime() - cvw.begin;
 if (++cvw.jobs % 300) return;
 char line[192];
 snprintf(line, sizeof(line), "[PS4 CVBLIT] jobs=%u worker_us=%llu cpu_mask=%x\n",
          cvw.jobs, cvw.sum/300, cvw.cpu_mask);
 sceKernelDebugOutText(0, line);
 cvw.sum=0; cvw.cpu_mask=0;
}
#else
static void cvw_reset() {}
static void cvw_begin() {}
static void cvw_end() {}
#endif
