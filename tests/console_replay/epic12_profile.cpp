#include <cassert>
#include <cstdint>
#include <cstring>
static unsigned long long ticks;
static unsigned clocks, logs, cpu_reads;
static uint64_t sceKernelGetProcessTime() { ++clocks; return ticks+=10; }
static int sceKernelGetCurrentCpu() { ++cpu_reads; return 3; }
static int sceKernelDebugOutText(int,const char *line) {
 ++logs;
 assert(strstr(line,"worker_us=10 cpu_mask=8"));
 return 0;
}
#define EPIC12_PROFILE_TEST
#include "../../src/burn/devices/epic12_ps4_profile.h"
int main() {
 cvw_reset();
 for (int i=0;i<7200;i++) { cvw_begin(); cvw_end(); }
 assert(logs==24 && clocks==14400 && cpu_reads==7200);
 for (int i=0;i<1000;i++) { cvw_begin(); cvw_end(); }
 assert(logs==24 && clocks==14400 && cpu_reads==7200);
 cvw_reset(); cvw_begin(); cvw_end();
 assert(logs==24 && clocks==14402 && cpu_reads==7201);
 puts("PASS: ordered-job averages, CPU mask, 24-report cap, no clocks/CPU reads after cap, load reset");
}
