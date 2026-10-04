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
static unsigned rng=0x48504;
static unsigned random32() { rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return rng; }
static unsigned callbacks, irq_cases, timer_cases;
static std::vector<UINT32> observations;
static void callback(int) { ++callbacks; observations.push_back(Sh3GetPC(-1)); observations.push_back(Sh3TotalCycles()); m_test_irq=1; }
#define CHECK(c) do { if(!(c)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c); exit(1); } } while(0)

static void compare(int budget, bool slice) {
 const std::vector<UINT8> before=save(); memcpy(saved_memory,memory,sizeof(memory));
 callbacks=0; observations.clear();
 const int old_cycles=slice?Sh3Run_timerhack(budget):Sh3Run_normal(budget);
 const unsigned old_callbacks=callbacks;
 const std::vector<UINT32> old_observations=observations;
 const std::vector<UINT8> expected=save(); memcpy(expected_memory,memory,sizeof(memory));
 restore(before); memcpy(memory,saved_memory,sizeof(memory)); callbacks=0; observations.clear();
 const int new_cycles=slice?Sh3Run_threaded<true>(budget,false):Sh3Run_threaded<false>(budget,false);
 CHECK(old_cycles==new_cycles); CHECK(old_callbacks==callbacks); CHECK(old_observations==observations);
 CHECK(expected==save()); CHECK(!memcmp(memory,expected_memory,sizeof(memory)));
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

int main() {
 Sh3Init(0,102400000,0,0,0,0,0,1,0,1,0);
 // Mirror a bounded backing store across the guest map, so arbitrary branch
 // targets and memory operands remain valid without installing fake handlers.
 for(unsigned i=0;i<SH3_PAGE_COUNT;++i) MemMapR[i]=MemMapW[i]=MemMapF[i]=memory;
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
  ++accepted;
  for(int trial=0;trial<3;++trial) {
   Sh3Reset(); m_pc=0x100; m_sr=random32();
   const Sh3OpcodeHandler handler=opcode_dispatch[op];
   const bool reads=handler==MOVBL || handler==MOVWL || handler==MOVLL || handler==MOVBP || handler==MOVWP || handler==MOVLP || handler==MOVWI || handler==MOVLI || handler==MOVBL0 || handler==MOVWL0 || handler==MOVLL0 || handler==MOVBL4 || handler==MOVWL4 || handler==MOVLL4;
   for(int r=0;r<16;++r) m_r[r]=reads?0x4000+r*16:random32();
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
