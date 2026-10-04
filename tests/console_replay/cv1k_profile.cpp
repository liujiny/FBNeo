#include <cassert>
#include <cstdint>
static unsigned long long ticks;
static unsigned clocks, logs, cpu_reads;
static int sceKernelGetCurrentCpu() { ++cpu_reads; return 2; }
static uint64_t sceKernelGetProcessTime() { ++clocks; return ticks+=10; }
static int sceKernelDebugOutText(int,const char *) { ++logs; return 0; }
#define CV1K_PROFILE_TEST
#include "../../src/burn/drv/cave/cv1k_ps4_profile.h"
int main() {
 cvp_reset();
 for(int i=0;i<7200;i++) {cvp_begin(); for(int p=0;p<4;p++)cvp_phase(p); cvp_end("test",7,2);}
 assert(logs==24 && clocks==36000 && cpu_reads==7200);
 unsigned before=clocks;
 for(int i=0;i<1000;i++) {cvp_begin(); for(int p=0;p<4;p++)cvp_phase(p);cvp_end("test",7,2);}
 assert(clocks==before && logs==24 && cpu_reads==7200);
 cvp_reset();cvp_begin();assert(clocks==before+1);
 puts("PASS: 300-frame windows, 24-report cap; no clocks/logs after cap; load reset");
}
