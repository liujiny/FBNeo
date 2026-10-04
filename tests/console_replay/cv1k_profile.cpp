#include <cassert>
#include <cstdint>
#include <string>
static unsigned long long ticks;
static unsigned clocks, logs, cpu_reads;
static int sceKernelGetCurrentCpu() { ++cpu_reads; return 2; }
static uint64_t sceKernelGetProcessTime() { ++clocks; return ticks+=10; }
static std::string last_log;
static int sceKernelDebugOutText(int,const char *line) { ++logs; last_log=line; return 0; }
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
 cvp_reset();
 for(int i=0;i<300;i++) {
  cvp_begin();
  for(int p=0;p<4;p++) {
   if(i==99 && p==0) ticks+=17000;
   if(i==199 && p==2) ticks+=21000;
   cvp_phase(p);
  }
  cvp_end("test",7,2);
 }
 assert(last_log.find("core_peak_us=21040 cpu_peak_us=17010 sync_peak_us=21010 core_over_16667=2 core_over_20000=1")!=std::string::npos);
 for(int i=0;i<300;i++) {cvp_begin(); for(int p=0;p<4;p++)cvp_phase(p); cvp_end("test",7,2);}
 assert(last_log.find("core_peak_us=40 cpu_peak_us=10 sync_peak_us=10 core_over_16667=0 core_over_20000=0")!=std::string::npos);
 puts("PASS: peak/over-budget counters and window reset; 300-frame windows, 24-report cap, unchanged syscall count and no clocks/logs after cap");
}
