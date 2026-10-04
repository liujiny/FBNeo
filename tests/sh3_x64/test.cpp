// Actual production loops, handlers and scanned device state form the oracle.
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define FBNEO_SH3_THREADED_DISPATCH 1
#define FBNEO_SH3_X64_JIT 1
#define FBNEO_SH3_JIT_TEST 1
#include "../../src/cpu/sh4/sh4.cpp"
#undef fprintf

static int quiet(INT32, TCHAR *, ...) { return 0; }
INT32 (__cdecl *bprintf)(INT32, TCHAR *, ...) = quiet;
void CpuCheatRegister(INT32, cpu_core_config *) {}
UINT8 serflash_io_read() { return 0; }
static std::vector<UINT8> snapshot;
static size_t offset;
static bool loading;
static int scan(BurnArea *area) {
 if (loading) memcpy(area->Data, &snapshot[offset], area->nLen);
 else snapshot.insert(snapshot.end(), (UINT8*)area->Data, (UINT8*)area->Data + area->nLen);
 offset += area->nLen; return 0;
}
INT32 (__cdecl *BurnAcb)(BurnArea *) = scan;
static std::vector<UINT8> save() { loading=false; offset=0; snapshot.clear(); Sh3Scan(0); return snapshot; }
static void restore(const std::vector<UINT8> &s) { snapshot=s; loading=true; offset=0; Sh3Scan(0); }
static UINT8 memory[65536], saved_memory[65536], expected_memory[65536];
static UINT8 write_page[65536], saved_write_page[65536], expected_write_page[65536];
static bool check_write_page;
static unsigned rng=0x48504;
static unsigned random32() { rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return rng; }
static unsigned callbacks, irq_cases, timer_cases;
static std::vector<UINT32> observations;
static void callback(int) { ++callbacks; observations.push_back(Sh3GetPC(-1)); observations.push_back(Sh3TotalCycles()); m_test_irq=1; }
#define CHECK(c) do { if(!(c)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c); exit(1); } } while(0)

static bool public_dispatch;
static void compare(int budget, bool slice) {
 Sh3SetTimerGranularity(slice ? 1 : 0);
 const std::vector<UINT8> before=save(); memcpy(saved_memory,memory,sizeof(memory));
 if(check_write_page) memcpy(saved_write_page,write_page,sizeof(write_page));
 callbacks=0; observations.clear();
 const int old_cycles=slice?Sh3Run_timerhack(budget):Sh3Run_normal(budget);
 const unsigned old_callbacks=callbacks;
 const std::vector<UINT32> old_observations=observations;
 const std::vector<UINT8> expected=save(); memcpy(expected_memory,memory,sizeof(memory));
 if(check_write_page) memcpy(expected_write_page,write_page,sizeof(write_page));
 restore(before); memcpy(memory,saved_memory,sizeof(memory)); callbacks=0; observations.clear();
 if(check_write_page) memcpy(write_page,saved_write_page,sizeof(write_page));
 Sh3SetTimerGranularity(slice ? 1 : 0);
 const int new_cycles=public_dispatch ? Sh3Run(budget) : (slice?Sh3Run_threaded<true, (FBNEO_SH3_X64_JIT != 0)>(budget,false):Sh3Run_threaded<false, false>(budget,false));
 CHECK(old_cycles==new_cycles); CHECK(old_callbacks==callbacks); CHECK(old_observations==observations);
 CHECK(expected==save()); CHECK(!memcmp(memory,expected_memory,sizeof(memory)));
 if(check_write_page) CHECK(!memcmp(write_page,expected_write_page,sizeof(write_page)));
 irq_cases += (m_sr & BL) != 0; timer_cases += old_callbacks != 0;
}

static UINT32 io_read(UINT32 address) {
 observations.push_back(address); observations.push_back(Sh3GetPC(-1));
 observations.push_back(Sh3TotalCycles()); Sh3BurnCycles(3);
 return Sh3TotalCycles() ^ Sh3GetPC(-1);
}
static void io_write(UINT32 address, UINT32 value) {
 observations.push_back(address); observations.push_back(value);
 observations.push_back(Sh3GetPC(-1)); observations.push_back(Sh3TotalCycles());
 Sh3BurnCycles(2);
}

static void alu_to_io_boundaries() {
 Sh3MapHandler(1,0x30000,0x3ffff,MAP_READ|MAP_WRITE);
 Sh3SetReadLongHandler(1,io_read);Sh3SetWriteLongHandler(1,io_write);
 const UINT16 program[]={0xe207,0x7201,0x2122,0x7201,0x6312,0x6323,
  0x7201,0x2122,0x7201,0x6312,0x0009,0x0009};
 for(int mode=0;mode<2;++mode) for(int delayed=0;delayed<2;++delayed)
  for(int budget=0;budget<=24;++budget) {
   Sh3Reset();m_pc=0x100;m_delay=delayed?0x80:0;m_sr=0;m_r[1]=0x30010;
   memcpy(memory+0x100,program,sizeof(program));((UINT16*)memory)[0x80/2]=0x7201;
   compare(budget,!!mode);
  }
 MemMapR[3]=MemMapW[3]=memory;
}

static void literal_cases() {
 // MOV.W/MOV.L PC-relative loads can cross a fetch-page boundary. The
 // address is constant, but neither the read mapping nor its data is frozen.
 const UINT16 loads[]={0x9120,0xd120};
 const UINT32 starts[]={0xffc0,0xa000ffc0};
 const int budgets[]={8,9,16,32};
 const unsigned long long before=Sh3X64::native_blocks;
 unsigned cases=0;
 for(unsigned op=0;op<2;++op) for(unsigned pc=0;pc<2;++pc) {
  Sh3Reset();
  for(unsigned i=0;i<32768;++i)((UINT16*)memory)[i]=0x0009;
  for(int i=0;i<32;++i)((UINT16*)memory)[0xffc0/2+i]=0x7201;
  ((UINT16*)memory)[0xffd0/2]=loads[op];
  for(int edit=0;edit<4;++edit) for(unsigned budget=0;budget<4;++budget) {
   for(unsigned i=0;i<256;++i) {
    write_page[i]=(UINT8)(i*19+edit*41+budget);
    memory[i]=(UINT8)(i*7+edit*23+budget);
   }
   MemMapR[1]=(edit&1)?write_page:memory;
   m_pc=starts[pc];m_sr=0;
   compare(budgets[budget],true);++cases;
  }
 }
 // The same PC-relative MOV.L must also leave native code before invoking
 // a handler, whose callback observes the exact PC and elapsed cycles.
 Sh3MapHandler(1,0x10000,0x1ffff,MAP_READ);
 Sh3SetReadLongHandler(1,io_read);
 m_pc=0xffc0;m_sr=0;compare(32,true);++cases;
 MemMapR[1]=memory;
 CHECK(Sh3X64::native_blocks>before);
 printf("PASS live PC-relative data/map/page/alias/device cases=%u\n",cases);
}

static void opcode_validation_cases() {
 const size_t page=sysconf(_SC_PAGESIZE);
 unsigned char *maps[2];
 for(int i=0;i<2;++i) {
  maps[i]=(unsigned char*)mmap(NULL,page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
  CHECK(maps[i]!=MAP_FAILED); CHECK(!mprotect(maps[i]+page,page,PROT_NONE));
 }
 unsigned cases=0;
 for(unsigned words=0;words<=Sh3X64::MAX_OPS+1;++words)
  for(unsigned pad_a=0;pad_a<8;++pad_a) for(unsigned pad_b=0;pad_b<8;++pad_b) {
   UINT16 *a=(UINT16*)(maps[0]+page)-(words+pad_a);
   UINT16 *b=(UINT16*)(maps[1]+page)-(words+pad_b);
   for(unsigned i=0;i<words;++i) a[i]=b[i]=(UINT16)(i*179+words*71);
   CHECK(Sh3X64::same_opcodes(a,b,words)); ++cases;
   // Every byte, including the high byte of the last word, is significant.
   for(unsigned i=0;i<words;++i) for(unsigned bit=0;bit<16;bit+=8) {
    b[i]^=(UINT16)(1<<bit); CHECK(!Sh3X64::same_opcodes(a,b,words));
    b[i]^=(UINT16)(1<<bit); ++cases;
   }
  }
 for(int i=0;i<2;++i) CHECK(!munmap(maps[i],page*2));
 printf("PASS bounded SSE2 opcode comparison/guard pages cases=%u\n",cases);
}

static void runtime_option_cases() {
 public_dispatch=true;
 unsigned cases=0;
 for(int mode=0;mode<2;++mode) for(int enabled=0;enabled<2;++enabled)
  for(int budget=0;budget<=40;++budget) {
   Sh3Reset();Sh3SetJitEnabled(enabled);m_pc=0x100;m_sr=0;
   for(int i=0;i<64;++i)((UINT16*)memory)[0x80+i]=0x7101;
   const unsigned long long before=Sh3X64::native_blocks, lookups=Sh3X64::lookups;
   compare(budget,!!mode);
   if(!enabled || !mode) CHECK(Sh3X64::lookups==lookups && Sh3X64::native_blocks==before);
   else if(budget>=32) CHECK(Sh3X64::native_blocks>before);
   ++cases;
  }
 // Re-enable an existing native block after live edits and a state restore.
 Sh3SetJitEnabled(1);m_pc=0x100;compare(32,true);
 const std::vector<UINT8> saved=save();
 Sh3SetJitEnabled(0);((UINT16*)memory)[0x80+20]=0x7107;m_pc=0x100;compare(32,true);
 restore(saved);Sh3SetJitEnabled(1);m_pc=0x100;compare(32,true);
 // Allocation/protection failures must route subsequent public runs through
 // the no-JIT specialization, without cache attempts and with exact timing.
 for(int mode=0;mode<2;++mode) {
  Sh3X64::release();Sh3X64::fail_allocation=mode==0;Sh3X64::fail_protection=mode==1;
  m_pc=0x100;compare(32,true);CHECK(Sh3X64::failed);
  const unsigned long long lookups=Sh3X64::lookups;
  m_pc=0x100;compare(32,true);CHECK(Sh3X64::lookups==lookups);
 }
 Sh3X64::fail_allocation=Sh3X64::fail_protection=false;Sh3X64::release();
 public_dispatch=false;Sh3SetJitEnabled(1);
 printf("PASS public dispatch on/off/failure and state/edit checks; budgets=%u\n",cases);
}

int main() {
 opcode_validation_cases();
 Sh3Init(0,102400000,0,0,0,0,0,1,0,1,0);
 CHECK(!Sh3X64::enabled);
 Sh3SetJitEnabled(1);
 // Mirror a bounded backing store across the guest map, so arbitrary branch
 // targets and memory operands remain valid without installing fake handlers.
 for(unsigned i=0;i<SH3_PAGE_COUNT;++i) MemMapR[i]=MemMapW[i]=MemMapF[i]=memory;
 runtime_option_cases();
 literal_cases();
 if(getenv("FBNEO_SH3_LITERAL_ONLY")) { Sh3Exit();return 0; }
 const UINT16 safe[]={0x0009,0x0018,0x0008,0xe123,0x7201,0x6123,0x312c,
  0x3128,0x2129,0x212a,0x212b,0x612c,0x612d,0x612e,0x612f,0x6118,
  0x6119,0x4110,0x4111,0x4115,0x4118,0x4119,0x411c,0x412d,
  0x2128,0x3120,0x3122,0x3123,0x3126,0x3127,0x4210,
  0x9120,0xd120,
  0x0019,0x0028,0x000a,0x012a,0x412a,0x3125};
 for(int slice=0;slice<2;++slice) {
  for(int trial=0;trial<1600;++trial) {
   Sh3Reset();
   for(unsigned i=0;i<32768;++i) ((UINT16*)memory)[i]=safe[random32()%(sizeof(safe)/sizeof(safe[0]))];
   for(int i=0;i<16;++i)m_r[i]=0x4000+i*16;
   m_pc=0x100; m_delay=(trial%3==0)?0x80:0; m_ppc=0x100;
   m_sr=(trial&1)?T:0; m_gbr=0x4000; m_vbr=0x1000;
   m_cpu_off=(trial%97==0);
   // Exercise actual pending interrupt entry and instruction-boundary deferral.
   if(trial%5==0) {sh4_exception_request(SH4_INTC_IRL2); m_test_irq=1;}
   if(trial%7==0) {
    m_timer[0].config(0,callback); m_timer[0].start(2,0,1,1);
    m_timer[0].timer_prescaler=ratio_multi;
   }
   // Branch, its delay slot and either direction of conditional branches.
   ((UINT16*)memory)[0x100/2]=(trial&1)?0xa002:0x8b02;
   ((UINT16*)memory)[0x102/2]=0x7101;
   compare(trial%79,!!slice);
  }
 }
 // Exercise memory handlers separately with aligned, bounded operands; random
 // ALU streams can create guest misalignment, whose legacy casts are undefined
 // in host C++ and would obscure validation of the dispatch change.
 const UINT16 memops[]={0x2120,0x2121,0x2122,0x2124,0x2125,0x2126,
  0x6120,0x6121,0x6122,0x6124,0x6125,0x6126,0x1313,0x5313,
  0x0124,0x0125,0x0126,0x012c,0x012d,0x012e,0x8510,0x8110};
 for(int slice=0;slice<2;++slice) for(unsigned i=0;i<sizeof(memops)/sizeof(memops[0]);++i) {
  Sh3Reset(); for(int r=0;r<16;++r)m_r[r]=0x4000+r*16;
  m_pc=0x100; ((UINT16*)memory)[0x80]=memops[i]; compare(1,!!slice);
 }
 alu_to_io_boundaries();
 // Guest code remains fetched from live memory, including edits between runs.
 Sh3Reset(); m_pc=0x100; ((UINT16*)memory)[0x80]=0xe107; compare(1,true);
 m_pc=0x100; ((UINT16*)memory)[0x80]=0xe10b; compare(1,true); CHECK(m_r[1]==11);

 // Execute each translated opcode with all encoded register pairs, including
 // aliases, arbitrary values and SR bits. Native execution is mandatory.
 unsigned accepted=0;
 for(unsigned op=0;op<65536;++op) {
  Sh3X64::Compiler probe;
  if(!probe.op(op)) continue;
  if(opcode_dispatch[op]==BT || opcode_dispatch[op]==BF || opcode_dispatch[op]==BTS || opcode_dispatch[op]==BFS) continue; // directed below
  ++accepted;
  for(int trial=0;trial<3;++trial) {
   Sh3Reset(); m_pc=0x100; m_sr=random32();
   const Sh3OpcodeHandler handler=opcode_dispatch[op];
   const bool reads=handler==MOVBL || handler==MOVWL || handler==MOVLL || handler==MOVBP || handler==MOVWP || handler==MOVLP || handler==MOVWI || handler==MOVLI || handler==MOVBL0 || handler==MOVWL0 || handler==MOVLL0 || handler==MOVBL4 || handler==MOVWL4 || handler==MOVLL4;
   const bool writes=handler==MOVBS || handler==MOVWS || handler==MOVLS || handler==MOVBM || handler==MOVWM || handler==MOVLM || handler==MOVBS0 || handler==MOVWS0 || handler==MOVLS0 || handler==MOVBS4 || handler==MOVWS4 || handler==MOVLS4;
   for(int r=0;r<16;++r) m_r[r]=(reads||writes)?0x4000+r*16:random32();
   if(writes) {
    bool small=handler==MOVBS4 || handler==MOVWS4;
    bool indexed=handler==MOVBS0 || handler==MOVWS0 || handler==MOVLS0;
    int addr=small?(op>>4)&15:(op>>8)&15, value=small?0:(op>>4)&15;
    if(value!=addr && !(indexed && value==0)) m_r[value]=random32();
   }
   for(int i=0;i<32;++i) ((UINT16*)memory)[0x80+i]=0x0009;
   ((UINT16*)memory)[0x80]=op;
   // Force compilation for this opcode-coverage test, including a PC that
   // previously cached an interpreter-only block. Runtime negative caching
   // intentionally does not eagerly discover newly compilable guest code.
   if(Sh3X64::blocks) {
    const unsigned set=((m_pc>>1)^(m_pc>>12)^(m_pc>>21))&(Sh3X64::CACHE_SETS-1);
    memset(Sh3X64::blocks+set*Sh3X64::WAYS,0,sizeof(Sh3X64::Block)*Sh3X64::WAYS);
   }
   const unsigned long long prior=Sh3X64::native_blocks;
   compare(32,true); CHECK(Sh3X64::native_blocks>prior);
  }
 }
 // Every conditional displacement, both directions, at multiple offsets and
 // budgets straddling native-region admission and taken-branch extra cycles.
 unsigned branches=0;
 for(unsigned op=0;op<65536;++op) {
  if(opcode_dispatch[op]!=BT && opcode_dispatch[op]!=BF && opcode_dispatch[op]!=BTS && opcode_dispatch[op]!=BFS) continue;
  for(int t=0;t<2;++t) for(int pos=0;pos<3;++pos) for(int budget=31;budget<=36;++budget) {
   Sh3Reset();m_pc=0x400;m_sr=0x700000f0|t;
   for(int i=0;i<32768;++i) ((UINT16*)memory)[i]=0x0009;
   ((UINT16*)memory)[0x200+(pos==0?0:pos==1?8:31)]=op;
   const unsigned long long prior=Sh3X64::native_blocks;
   compare(budget,true);
   if(budget>=34) CHECK(Sh3X64::native_blocks>prior);
   ++branches;
  }
 }
 printf("PASS conditional branch encodings/boundaries=%u\n",branches);
 // Nontrivial delay slots: all supported arithmetic encodings and an I/O
 // fallback. Each taken slot must execute once before entering the target.
 unsigned delay_cases=0;
 for(unsigned op=0;op<65536;++op) {
  Sh3X64::Compiler probe;
  if(!probe.alu(op)) continue;
  Sh3Reset();m_pc=0x400;m_sr=1;
  for(int r=0;r<16;++r) m_r[r]=random32();
  for(int i=0;i<64;++i) ((UINT16*)memory)[0x200+i]=0x0009;
  ((UINT16*)memory)[0x200]=0x8d10;((UINT16*)memory)[0x201]=op;
  compare(36,true);++delay_cases;
 }
 Sh3MapHandler(1,0x30000,0x3ffff,MAP_READ|MAP_WRITE);
 Sh3SetReadLongHandler(1,io_read);Sh3SetWriteLongHandler(1,io_write);
 for(int pos=0;pos<32;++pos) for(int t=0;t<2;++t) {
  Sh3Reset();m_pc=0x400;m_sr=t;m_r[1]=0x30010;m_r[2]=0x12345678;
  for(int i=0;i<96;++i) ((UINT16*)memory)[0x200+i]=0x0009;
  ((UINT16*)memory)[0x200+pos]=0x8d20;((UINT16*)memory)[0x201+pos]=0x2122;
  compare(40,true);++delay_cases;
 }
 MemMapR[3]=MemMapW[3]=memory;
 printf("PASS native and device delay slots=%u\n",delay_cases);
 // A native store must see current write mappings and must not execute a
 // stale following opcode, including physical/host aliases and predecrement.
 const UINT16 stores[]={0x2120,0x2121,0x2122,0x2124,0x2125,0x2126,0x1120};
 unsigned store_cases=0;
 for(unsigned s=0;s<sizeof(stores)/sizeof(stores[0]);++s)
  for(int alias=0;alias<3;++alias) for(int pos=0;pos<2;++pos) {
   Sh3Reset();m_pc=0x100;m_sr=0;
   for(int i=0;i<32768;++i) ((UINT16*)memory)[i]=0x0009;
   for(int r=0;r<16;++r) m_r[r]=0x4000+r*16;
   const Sh3OpcodeHandler h=opcode_dispatch[stores[s]];
   const bool pre=h==MOVBM || h==MOVWM || h==MOVLM;
   int width=h==MOVBS || h==MOVBM?1:h==MOVWS || h==MOVWM?2:4;
   UINT32 target=0x120+(alias==1?0x10000:alias==2?0xa0000000:0);
   m_r[1]=target+(pre?width:0);m_r[2]=width==4?0xe5070009:width==2?0xe507:0xe5;
   ((UINT16*)memory)[0x80+(pos?8:0)]=stores[s];
   compare(40,true);++store_cases;
  }
 // A handler-mapped write must retain exact callback PC/cycles and values.
 Sh3MapHandler(1,0x30000,0x3ffff,MAP_READ|MAP_WRITE);
 Sh3SetWriteLongHandler(1,io_write);
 for(int pos=0;pos<32;++pos) {
  Sh3Reset();m_pc=0x100;m_sr=0;m_r[1]=0x30010;m_r[2]=0x12345678;
  for(int i=0;i<64;++i) ((UINT16*)memory)[0x80+i]=0x7201;
  ((UINT16*)memory)[0x80+pos]=0x2122;compare(36,true);++store_cases;
 }
 MemMapR[3]=MemMapW[3]=memory;
 // Reuse the exact same compiled block after changing only the write map.
 // The first and second maps intentionally differ from the fetch/read map.
 check_write_page=true;
 for(int remap=0;remap<3;++remap) {
  Sh3Reset();m_pc=0x100;m_sr=0;m_r[1]=0x4010;m_r[2]=0x80ff7f01;
  for(int i=0;i<64;++i) ((UINT16*)memory)[0x80+i]=0x0009;
  ((UINT16*)memory)[0x88]=0x2122;
  MemMapW[0]=remap==1?write_page:memory;
  compare(32,true);++store_cases;
 }
 MemMapW[0]=memory;check_write_page=false;
 printf("PASS store self-modification/alias/device cases=%u\n",store_cases);
 // Carry/overflow and dynamic shift edge values, in addition to random
 // full-encoding coverage. This executes native code, not an emitter model.
 const UINT32 edges[]={0,1,0xffffffff,0x80000000,0x7fffffff,31,32,0xffffffe0};
 unsigned edge_cases=0;
 for(unsigned op=0;op<65536;++op) {
  if(((op>>8)&15)!=1 || ((op>>4)&15)!=2) continue;
  Sh3OpcodeHandler h=opcode_dispatch[op];
  if(h!=ADDC && h!=SUBC && h!=ADDV && h!=SUBV && h!=NEGC && h!=ROTCL && h!=ROTCR && h!=SHAD && h!=SHLD && h!=CMPSTR && h!=XTRCT) continue;
  for(unsigned a=0;a<8;++a) for(unsigned b=0;b<8;++b) for(unsigned t=0;t<2;++t) {
   Sh3Reset();m_pc=0x100;m_sr=0x700000f0|t;m_r[1]=edges[a];m_r[2]=edges[b];
   for(int i=0;i<32;++i) ((UINT16*)memory)[0x80+i]=0x0009;
   ((UINT16*)memory)[0x80]=op;compare(32,true);++edge_cases;
  }
 }
 printf("PASS native carry/overflow/shift edge cases=%u\n",edge_cases);
 // Entry validation observes changes beyond opcode zero and previously
 // unsupported terminators; code can also arrive through another fetch map.
 Sh3Reset(); m_pc=0x100; m_sr=0;
 for(int i=0;i<32;++i) ((UINT16*)memory)[0x80+i]=0x7101;
 compare(32,true);
 m_pc=0x100; ((UINT16*)memory)[0x80+20]=0x7107; compare(32,true);
 m_pc=0x100; ((UINT16*)memory)[0x80+3]=0xffff; compare(32,true);
 m_pc=0x100; ((UINT16*)memory)[0x80+3]=0x7102; compare(32,true);
 static UINT8 other_page[65536]; memcpy(other_page,memory,sizeof(memory));
 ((UINT16*)other_page)[0x80]=0x7109; MemMapF[0]=other_page;
 m_pc=0x100; compare(32,true); MemMapF[0]=memory;
 // End-of-page decoding never reads the next page through this host pointer.
 m_pc=0xfff8; for(int i=0;i<4;++i) ((UINT16*)memory)[0x7ffc+i]=0x7101;
 compare(4,true);
 // Allocation/protection failure is a fail-closed interpreter fallback.
 for(int mode=0;mode<2;++mode) {
  Sh3Exit(); Sh3X64::fail_allocation=mode==0; Sh3X64::fail_protection=mode==1;
  m_pc=0x100; const unsigned long long prior=Sh3X64::native_blocks;
  compare(32,true); CHECK(Sh3X64::native_blocks==prior); CHECK(Sh3X64::failed);
 }
 Sh3X64::fail_allocation=Sh3X64::fail_protection=false; Sh3Exit();
 m_pc=0x100; compare(32,true); CHECK(Sh3X64::native_blocks>0);
 printf("PASS JIT opcode encodings=%u, native blocks=%llu, native ops=%llu, builds=%llu\n",accepted,Sh3X64::native_blocks,Sh3X64::native_ops,Sh3X64::builds);
 Sh3Exit(); CHECK(!Sh3X64::code && !Sh3X64::blocks);
 CHECK(irq_cases > 0 && timer_cases > 0);
 puts("PASS 3200 randomized and 146 directed production-loop comparisons: full CPU/device state, RAM, cycles, delayed branches, IRQ, timers, stopped CPU and live opcode edits");
 return 0;
}
