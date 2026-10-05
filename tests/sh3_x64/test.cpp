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


static UINT16 dt_peek_value;
static UINT16 dt_read(UINT32 address) {
 observations.push_back(address); observations.push_back(Sh3GetPC(-1));
 observations.push_back(Sh3TotalCycles()); observations.push_back(m_r[2]);
 observations.push_back(m_sr); Sh3BurnCycles(1); return dt_peek_value;
}
static void dt_cases() {
 public_dispatch=true;Sh3SetJitEnabled(1);Sh3X64::release();
 Sh3Reset();m_pc=0x100;m_sr=0;m_r[2]=2;
 for(unsigned i=0;i<32768;++i)((UINT16*)memory)[i]=0x0009;
 ((UINT16*)memory)[0x110/2]=0x4210;
 const unsigned long long before=Sh3X64::native_ops;
 compare(32,true);CHECK(Sh3X64::native_ops-before==32);
 const UINT32 starts[]={0x100,0xffee,0xa0000100};
 const UINT32 values[]={0,1,2,3,7,0x80000000,0xffffffff};
 const int budgets[]={0,1,7,8,9,10,16,31,32,33,40,64};
 Sh3SetReadWordHandler(1,dt_read);
 unsigned cases=0;
 for(unsigned pc=0;pc<3;++pc) for(unsigned val=0;val<7;++val)
 for(int map=0;map<3;++map) for(int busy=0;busy<2;++busy)
 for(int slice=0;slice<2;++slice) for(unsigned budget=0;budget<12;++budget) {
  MemMapR[0]=MemMapR[1]=memory;Sh3Reset();
  for(unsigned i=0;i<32768;++i)((UINT16*)memory)[i]=((UINT16*)write_page)[i]=0x0009;
  m_pc=starts[pc];m_sr=T;m_r[2]=values[val];m_r[1]=0;
  for(unsigned i=0;i<8;++i)((UINT16*)memory)[((starts[pc]+i*2)&0xffff)/2]=0x7101;
  const UINT32 dt=starts[pc]+16,next=(dt+2)&AM;
  ((UINT16*)memory)[(dt&0xffff)/2]=0x4210;
  dt_peek_value=busy?0x8bfd:0x0009;
  if(map==0)((UINT16*)memory)[(next&0xffff)/2]=dt_peek_value;
  if(map==1) {MemMapR[next>>SH3_SHIFT]=write_page;((UINT16*)write_page)[(next&0xffff)/2]=dt_peek_value;}
  if(map==2) MemMapR[next>>SH3_SHIFT]=(UINT8*)1;
  compare(budgets[budget],!!slice);++cases;
 }
 MemMapR[0]=MemMapR[1]=memory;
 // DT in a taken delayed branch observes the branch target via m_ppc.
 for(int branch=0;branch<2;++branch) for(int busy=0;busy<2;++busy)
 for(unsigned budget=0;budget<12;++budget) {
  Sh3Reset();m_pc=0x100;m_sr=branch?T:0;m_r[2]=7;
  for(unsigned i=0;i<32768;++i)((UINT16*)memory)[i]=0x0009;
  ((UINT16*)memory)[0x100/2]=branch?0x8d08:0x8f08;
  ((UINT16*)memory)[0x102/2]=0x4210;
  ((UINT16*)memory)[0x114/2]=busy?0x8bfd:0x0009;
  compare(budgets[budget],true);++cases;
 }
 public_dispatch=false;
 printf("PASS DT native/busy/READ-vs-FETCH/map/page/alias/delay cases=%u\n",cases);
}


static void cold_guard_cases() {
 public_dispatch=true;Sh3SetJitEnabled(1);Sh3X64::release();
 unsigned cases=0;
 const UINT16 ops[]={0x6122,0x6126,0x2122,0x2126,0x6210};
 // Many dirty guest registers force cache spills before each guarded exit;
 // snapshots must retain the allocation at that exit, not the final block.
 for(unsigned kind=0;kind<4;++kind) for(int pos=0;pos<25;++pos)
 for(int mapping=0;mapping<3;++mapping) for(int branch=0;branch<2;++branch) {
  MemMapR[3]=MemMapW[3]=memory;Sh3Reset();
  for(unsigned i=0;i<32768;++i)((UINT16*)memory)[i]=0x0009;
  for(int i=0;i<16;++i)m_r[i]=0x4000+i*16;
  m_pc=0x100;m_sr=0;
  for(int i=0;i<pos;++i)((UINT16*)memory)[0x80+i]=0x7001|((4+i%12)<<8);
  if(branch && pos>0)((UINT16*)memory)[0x80+pos/2]=0x8902; // untaken, keeps fallthrough
  ((UINT16*)memory)[0x80+pos]=ops[kind];
  for(int i=pos+1;i<32;++i)((UINT16*)memory)[0x80+i]=0x7003|((4+i%12)<<8);
  const int address=kind<2?2:1;
  m_r[address]=mapping==0?0x4010:mapping==1?0x30010:0x10000+0x100+(pos+1)*2;
  if(kind==3)m_r[address]+=4;
  // Long stores require aligned addresses; the alias still targets future code.
  m_r[address]&=~3U;
  if(kind>=2)m_r[2]=0x00090009;
  if(mapping==1) {
   Sh3MapHandler(1,0x30000,0x3ffff,MAP_READ|MAP_WRITE);
   Sh3SetReadLongHandler(1,io_read);Sh3SetWriteLongHandler(1,io_write);
  }
  compare(40,true);++cases;
 }
 MemMapR[3]=MemMapW[3]=memory;
 // Multiple guards in one region, with only a later map reaching a handler.
 for(int failing=0;failing<16;++failing) for(int prefix=0;prefix<8;++prefix) {
  Sh3Reset();m_pc=0x100;m_sr=0;
  for(unsigned i=0;i<32768;++i)((UINT16*)memory)[i]=0x0009;
  for(int i=0;i<16;++i)m_r[i]=0x4000+i*16;
  m_r[2]=0x4000;m_r[3]=0x30010;
  for(int i=0;i<32;++i) {
   ((UINT16*)memory)[0x80+i]=(i%2)?(0x7001|((4+(i+prefix)%12)<<8)):0x6122;
  }
  ((UINT16*)memory)[0x80+failing*2]=0x6132;
  Sh3MapHandler(1,0x30000,0x3ffff,MAP_READ);Sh3SetReadLongHandler(1,io_read);
  compare(40,true);++cases;MemMapR[3]=memory;
 }
 Sh3X64::release();public_dispatch=false;
 printf("PASS cold guard register snapshots/spills/maps/callback/alias cases=%u\n",cases);
}


static UINT32 mac_io_read(UINT32 address) {
 observations.push_back(m_mach);observations.push_back(m_macl);
 for(int i=0;i<16;++i)observations.push_back(m_r[i]);
 return io_read(address);
}
static void mac_io_write(UINT32 address, UINT32 value) {
 observations.push_back(m_mach);observations.push_back(m_macl);
 for(int i=0;i<16;++i)observations.push_back(m_r[i]);
 io_write(address,value);
}
static void multiply_cases() {
 CHECK(opcode_dispatch[0x0127]==MULL);
 CHECK(opcode_dispatch[0x011a]==STSMACL && opcode_dispatch[0x010a]==STSMACH);
 CHECK(opcode_dispatch[0x411a]==LDSMACL && opcode_dispatch[0x410a]==LDSMACH);
 public_dispatch=true;Sh3SetJitEnabled(1);Sh3X64::release();Sh3Reset();
 for(unsigned i=0;i<32768;++i)((UINT16*)memory)[i]=0x0009;
 for(int i=0;i<32;++i)((UINT16*)memory)[0x80+i]=0x0127;
 m_pc=0x100;m_sr=0;m_r[1]=0xffffffff;m_r[2]=0x80000000;
 unsigned long long n=Sh3X64::native_ops;compare(64,true);CHECK(Sh3X64::native_ops==n+32);
 unsigned cases=1;
 const UINT32 values[]={0,1,2,0xffff,0xffffffff,0x80000000,0x7fffffff,0xdeadbeef};
 for(unsigned a=0;a<8;++a) for(unsigned b=0;b<8;++b) for(int alias=0;alias<2;++alias) {
  Sh3Reset();m_pc=0x100;m_sr=0x700000f1;m_r[1]=values[a];m_r[2]=values[b];
  m_mach=0x12345678;m_macl=0x87654321;
  for(int i=0;i<64;++i)((UINT16*)memory)[0x80+i]=0x0009;
  const UINT16 program[]={0x0127,0x041a,0x040a,0x441a,0x4410,0x440a,0x051a};
  memcpy(memory+0x100,program,sizeof(program));if(alias)((UINT16*)memory)[0x80]=0x0117;
  // The DT peek is ordinary NOP, so this also mixes MAC and guarded ops.
  compare(40,true);++cases;
 }
 const int counts[]={1,4,16};
 const int budgets[]={0,1,2,7,8,15,16,31,32,33,34,40,48,64,96};
 for(unsigned c=0;c<3;++c) for(int event=0;event<7;++event)
 for(unsigned b=0;b<15;++b) for(int taken=0;taken<2;++taken) {
  MemMapR[3]=MemMapW[3]=memory;Sh3Reset();m_pc=0x100;m_sr=taken;
  for(unsigned i=0;i<32768;++i)((UINT16*)memory)[i]=0x0009;
  for(int i=0;i<16;++i)m_r[i]=0x4000+i*16;
  m_r[1]=0xfedcba98;m_r[2]=0x76543210;m_r[3]=0x30010;m_r[7]=3;
  m_mach=0x11223344;m_macl=0x55667788;
  for(int i=0;i<counts[c];++i)((UINT16*)memory)[0x80+i]=0x0127;
  ((UINT16*)memory)[0x80+counts[c]]=0x041a;
  const unsigned at=0x80+counts[c]+1;
  if(event==1)((UINT16*)memory)[at]=0x6632;
  if(event==2)((UINT16*)memory)[at]=0x2362;
  if(event==3)((UINT16*)memory)[at]=0x8902;
  if(event==4 || event==5) {((UINT16*)memory)[at]=0x8d02;((UINT16*)memory)[at+1]=event==4?0x0127:0x481a;}
  if(event==6) {((UINT16*)memory)[at]=0x4710;((UINT16*)memory)[at+1]=0x8bfd;}
  Sh3MapHandler(1,0x30000,0x3ffff,MAP_READ|MAP_WRITE);
  Sh3SetReadLongHandler(1,mac_io_read);Sh3SetWriteLongHandler(1,mac_io_write);
  compare(budgets[b],true);++cases;
 }
 MemMapR[3]=MemMapW[3]=memory;Sh3SetReadLongHandler(1,io_read);Sh3SetWriteLongHandler(1,io_write);
 Sh3X64::release();public_dispatch=false;
 printf("PASS native multiply/MAC/extra-cycles/branch/delay/DT/callback cases=%u\n",cases);
}


static void native_dispatch_loop_cases() {
 public_dispatch=true;Sh3SetJitEnabled(1);unsigned cases=0;
 // Cross many adjacent native regions and straddle both exact native budget
 // exhaustion and an interpreter remainder. Zero budgets must still execute
 // the legacy initial instruction instead of being mistaken for completion.
 for(int multiply=0;multiply<2;++multiply) for(int budget=0;budget<=257;++budget) {
  Sh3Reset();m_pc=0x100;m_sr=0;m_r[1]=0xfedcba98;m_r[2]=7;
  for(unsigned i=0;i<32768;++i)((UINT16*)memory)[i]=multiply?0x0127:0x7101;
  compare(budget,true);++cases;
 }
 // A zero-completed native guard after several completed regions must stop
 // dispatching and invoke the device exactly once at its original PC/cycle.
 for(int region=0;region<5;++region) {
  Sh3Reset();m_pc=0x100;m_sr=0;m_r[2]=0x30010;
  for(unsigned i=0;i<32768;++i)((UINT16*)memory)[i]=0x7101;
  ((UINT16*)memory)[0x80+region*32]=0x6322;
  Sh3MapHandler(1,0x30000,0x3ffff,MAP_READ);Sh3SetReadLongHandler(1,mac_io_read);
  compare(192,true);++cases;MemMapR[3]=memory;
 }
 Sh3SetReadLongHandler(1,io_read);Sh3X64::release();public_dispatch=false;
 printf("PASS internal native dispatch boundaries/zero budget/device cases=%u\n",cases);
}


static void arena_capacity_cases() {
 CHECK(Sh3X64::CODE_BYTES==8*1024*1024);
 public_dispatch=true;Sh3SetJitEnabled(1);Sh3X64::release();Sh3Reset();
 for(unsigned i=0;i<32768;++i)((UINT16*)memory)[i]=0x7101;
 m_pc=0x100;m_sr=0;compare(32,true);
 const unsigned set=((0x100>>1)^(0x100>>12)^(0x100>>21))&(Sh3X64::CACHE_SETS-1);
 bool found=false;
 for(unsigned w=0;w<Sh3X64::WAYS;++w)if(Sh3X64::blocks[set*Sh3X64::WAYS+w].entry)found=true;
 CHECK(found);
 m_pc=0x200;compare(32,true);
 Sh3X64::used=Sh3X64::CODE_BYTES-Sh3X64::SLOT_BYTES+1;
 m_pc=0x300;const unsigned long long n=Sh3X64::native_ops;compare(32,true);
 CHECK(Sh3X64::native_ops==n+32 && Sh3X64::used<Sh3X64::SLOT_BYTES);
 for(unsigned w=0;w<Sh3X64::WAYS;++w)CHECK(!Sh3X64::blocks[set*Sh3X64::WAYS+w].entry);
 ((UINT16*)memory)[0x80+9]=0x7107;m_pc=0x100;compare(32,true);
 Sh3X64::release();CHECK(!Sh3X64::code && !Sh3X64::blocks && !Sh3X64::used);public_dispatch=false;
 puts("PASS 8MiB arena boundary/recycle/stale-entry/edit/release");
}

static void ram_run_cases() {
 const unsigned long long before=sh3_ram_run_ops;
 const UINT16 ops[]={0x5120,0x1120,0x6126,0x2126,0xd120,0x5110,0x1110,0x6116,0x2116};
 const UINT32 addr[]={0x4000,0x4001,0xfffc,0x10000,0xa0004000,0x30010};
 unsigned cases=0;public_dispatch=true;
 Sh3MapHandler(1,0x30000,0x3ffff,MAP_READ|MAP_WRITE);Sh3SetReadLongHandler(1,io_read);Sh3SetWriteLongHandler(1,io_write);
 for(unsigned op=0;op<9;++op) for(unsigned a=0;a<6;++a) for(int jit=0;jit<2;++jit)
 for(int delayed=0;delayed<2;++delayed) for(int budget=0;budget<=40;++budget) {
#if defined(__SANITIZE_ADDRESS__)
  // The unchanged reference RL uses a typed unaligned dereference (UB).
  // Exercise its x86 fallback separately in the non-sanitized suite.
  if(addr[a]&3) continue;
#endif
  Sh3Reset();Sh3SetJitEnabled(jit);m_pc=0x400;m_sr=0;m_ea=0x76543210;
  for(unsigned i=0;i<32768;++i)((UINT16*)memory)[i]=0x0009;
  for(int r=0;r<16;++r)m_r[r]=addr[a];
  // A later handler write must observe the fully committed prefix.
  m_r[15]=0x30010;
  for(int i=0;i<64;++i)((UINT16*)memory)[0x200+i]=0x7201;
  ((UINT16*)memory)[0x208]=ops[op];((UINT16*)memory)[0x209]=0x2f22;
  if(delayed) {m_delay=0x410;m_pc=0x440;}
  compare(budget,true);++cases;
 }
 // Pure RAM stores changing the following instruction must fetch it live.
 for(int jit=0;jit<2;++jit) for(int alias=0;alias<3;++alias) for(int budget=1;budget<40;++budget) {
  Sh3Reset();Sh3SetJitEnabled(jit);m_pc=0x400;m_sr=0;
  for(unsigned i=0;i<32768;++i)((UINT16*)memory)[i]=0x0009;
  m_r[1]=0x410+(alias==1?0x10000:alias==2?0xa0000000:0);m_r[2]=0xe5070009;
  ((UINT16*)memory)[0x204]=0x1120;compare(budget,true);++cases;
 }
 MemMapR[3]=MemMapW[3]=memory;public_dispatch=false;Sh3SetJitEnabled(1);
 CHECK(sh3_ram_run_ops>before);
 printf("PASS RAM runs aligned/fallback/alias/callback/delay/selfmod/JIT modes=%u\n",cases);
}
static void ram_run_random_cases() {
 public_dispatch=true;unsigned cases=0;const unsigned long long before=sh3_ram_run_ops;
 Sh3MapHandler(1,0x30000,0x3ffff,MAP_READ|MAP_WRITE);Sh3SetWriteLongHandler(1,io_write);
 for(int trial=0;trial<600;++trial) {
  Sh3Reset();Sh3SetJitEnabled(0);m_pc=0x400;m_sr=random32();m_ea=random32();
  for(unsigned i=0;i<32768;++i)((UINT16*)memory)[i]=0x0009;
  for(unsigned i=0x3000/4;i<0x7000/4;++i)((UINT32*)memory)[i]=0x40000000;
  for(int r=0;r<16;++r)m_r[r]=0x4000+16*r;m_r[15]=0x30010;
  for(int i=0;i<512;++i) {
   const unsigned n=random32()%8,m=random32()%8,disp=random32()%16;
   const UINT16 ops[]={UINT16(0x5000|(n<<8)|(m<<4)|disp),UINT16(0x1000|(n<<8)|(m<<4)|disp),
    UINT16(0x6006|(n<<8)|(m<<4)),UINT16(0x2006|(n<<8)|(m<<4)),UINT16(0x7004|(n<<8)),
    UINT16(0x6003|(n<<8)|(m<<4)),UINT16(0x3000|(n<<8)|(m<<4)),0xde20,0x8b00,0x8900};
   ((UINT16*)memory)[0x200+i]=ops[random32()%10];
  }
  ((UINT16*)memory)[0x200+128]=0x2f02;
  if(trial&1){m_delay=0x400;m_pc=0x420;}
  compare(1+random32()%511,true);++cases;
 }
 MemMapR[3]=MemMapW[3]=memory;public_dispatch=false;Sh3SetJitEnabled(1);
 CHECK(sh3_ram_run_ops>before);printf("PASS randomized interpreter-only mixed RAM/ALU/branch/delay/callback cases=%u\n",cases);
}
static inline void DIV1_reference(const UINT16 opcode)
{
	UINT32 m = Rm; UINT32 n = Rn;

	UINT32 tmp0;
	UINT32 old_q;

	old_q = m_sr & Q;
	if (0x80000000 & m_r[n])
		m_sr |= Q;
	else
		m_sr &= ~Q;

	m_r[n] = (m_r[n] << 1) | (m_sr & T);

	if (!old_q)
	{
		if (!(m_sr & M))
		{
			tmp0 = m_r[n];
			m_r[n] -= m_r[m];
			if(!(m_sr & Q))
				if(m_r[n] > tmp0)
					m_sr |= Q;
				else
					m_sr &= ~Q;
			else
				if(m_r[n] > tmp0)
					m_sr &= ~Q;
				else
					m_sr |= Q;
		}
		else
		{
			tmp0 = m_r[n];
			m_r[n] += m_r[m];
			if(!(m_sr & Q))
			{
				if(m_r[n] < tmp0)
					m_sr &= ~Q;
				else
					m_sr |= Q;
			}
			else
			{
				if(m_r[n] < tmp0)
					m_sr |= Q;
				else
					m_sr &= ~Q;
			}
		}
	}
	else
	{
		if (!(m_sr & M))
		{
			tmp0 = m_r[n];
			m_r[n] += m_r[m];
			if(!(m_sr & Q))
				if(m_r[n] < tmp0)
					m_sr |= Q;
				else
					m_sr &= ~Q;
			else
				if(m_r[n] < tmp0)
					m_sr &= ~Q;
				else
					m_sr |= Q;
		}
		else
		{
			tmp0 = m_r[n];
			m_r[n] -= m_r[m];
			if(!(m_sr & Q))
				if(m_r[n] > tmp0)
					m_sr &= ~Q;
				else
					m_sr |= Q;
			else
				if(m_r[n] > tmp0)
					m_sr |= Q;
				else
					m_sr &= ~Q;
		}
	}

	tmp0 = (m_sr & (Q | M));
	if((!tmp0) || (tmp0 == 0x300)) /* if Q == M set T else clear T */
		m_sr |= T;
	else
		m_sr &= ~T;
}


static void div1_oracle_case(UINT16 opcode,UINT32 sr,UINT32 a,UINT32 b) {
 for(unsigned i=0;i<16;++i)m_r[i]=0x11223300+i;
 m_r[(opcode>>8)&15]=a;m_r[(opcode>>4)&15]=b;m_sr=sr;
 UINT32 initial[16],expected[16];memcpy(initial,m_r,sizeof(initial));
 DIV1_reference(opcode);memcpy(expected,m_r,sizeof(expected));const UINT32 expected_sr=m_sr;
 memcpy(m_r,initial,sizeof(initial));m_sr=sr;DIV1(opcode);
 CHECK(m_sr==expected_sr);CHECK(!memcmp(m_r,expected,sizeof(expected)));
}
static void div1_oracle_cases() {
 const UINT32 values[]={0,1,2,0x7fffffff,0x80000000U,0xfffffffeU,0xffffffffU,0x40000000,0xc0000000U};
 unsigned cases=0;
 for(unsigned n=0;n<16;++n)for(unsigned m=0;m<16;++m)for(unsigned bits=0;bits<8;++bits)
  for(unsigned a=0;a<9;++a)for(unsigned b=0;b<9;++b) {
   const UINT32 sr=(0xf00f00f0U&~(Q|M|T))|((bits&1)?T:0)|((bits&2)?Q:0)|((bits&4)?M:0);
   div1_oracle_case((UINT16)(0x3004|(n<<8)|(m<<4)),sr,values[a],values[b]);++cases;
  }
 for(unsigned i=0;i<200000;++i){div1_oracle_case((UINT16)(0x3004|(random32()&0x0ff0)),random32(),random32(),random32());++cases;}
 printf("PASS DIV1 original-handler oracle all register aliases/flags/edge/random cases=%u\n",cases);
}


static UINT8 byte_io_read(UINT32 address) {
 observations.push_back(address);observations.push_back(Sh3GetPC(-1));
 observations.push_back(Sh3TotalCycles());Sh3BurnCycles(2);return (UINT8)address;
}
static void byte_io_write(UINT32 address,UINT8 value) {
 observations.push_back(address);observations.push_back(value);observations.push_back(Sh3GetPC(-1));
 observations.push_back(Sh3TotalCycles());Sh3BurnCycles(3);
}
static void byte_setup(bool copy,unsigned rotation,UINT32 pc=0x100) {
 for(unsigned i=0;i<SH3_PAGE_COUNT;++i)MemMapR[i]=MemMapW[i]=MemMapF[i]=memory;
 Sh3Reset();
 for(unsigned i=0;i<32768;++i)((UINT16*)memory)[i]=0x0009;
 for(unsigned i=0x2000;i<0x2100;++i)memory[i]=(UINT8)(i*47);
 unsigned v=rotation,s=(rotation+5)&15,d=(rotation+4)&15,c=(rotation+6)&15;
 UINT16 ops[7];unsigned n=0;
 if(copy)ops[n++]=0x6004|(v<<8)|(s<<4);
 ops[n++]=0x2000|(d<<8)|((copy?v:s)<<4);ops[n++]=0x4010|(c<<8);
 ops[n++]=copy?0x8ffb:0x8ffc;ops[n++]=0x7001|(d<<8);ops[n++]=0x000b;ops[n++]=0x0009;
 for(unsigned i=0;i<n;++i)((UINT16*)memory)[((pc+2*i)&65535)/2]=ops[i];
 m_pc=pc;m_pr=0x800;m_sr=0;m_ea=0x12345678;
 m_r[s]=0x2000;m_r[d]=0x4000;m_r[c]=32;
}
static void byte_loop_cases() {
 public_dispatch=true;unsigned cases=0;
 const int budgets[]={0,1,2,3,4,5,6,7,8,9,10,11,12,17,31,64,128,256};
 const UINT32 counts[]={0,1,2,3,16,32,0xffffffff};
 for(int copy=0;copy<2;++copy)for(int jit=0;jit<2;++jit)for(int mode=0;mode<2;++mode)
 for(unsigned val=0;val<7;++val)for(unsigned b=0;b<18;++b)for(int parity=0;parity<4;++parity) {
  byte_setup(copy,0);Sh3SetJitEnabled(jit);m_r[6]=counts[val];m_r[5]+=parity&1;m_r[4]+=(parity>>1);
  const unsigned long long before=sh3_byte_loop_calls;
  compare(budgets[b],mode);++cases;
  if(!mode || !jit)CHECK(before==sh3_byte_loop_calls);
  if(mode && jit && budgets[b]>=64 && counts[val]>=16)CHECK(sh3_byte_loop_calls>before);
 }
 for(int copy=0;copy<2;++copy)for(unsigned rot=0;rot<16;++rot)for(int entry=0;entry<4;++entry)
 for(unsigned b=0;b<18;++b) {
  byte_setup(copy,rot);Sh3SetJitEnabled(b&1);
  if(entry) {m_pc=0x100+(copy?6:4);if(entry==2)m_sr|=T;
   if(entry==3){m_delay=0x600;((UINT16*)memory)[0x300]=copy?0x8ffb:0x8ffc;m_pc+=2;}}
  compare(budgets[b],true);++cases;
 }
 // Source/destination page ends and FETCH crossing a page; live map aliases.
 for(int copy=0;copy<2;++copy)for(int which=0;which<6;++which)for(unsigned b=0;b<18;++b) {
  byte_setup(copy,0,which==0?0xfffa:0x100);Sh3SetJitEnabled(b&1);
  if(which==1)m_r[5]=0xfffd;if(which==2)m_r[4]=0xfffd;
  if(which==3){m_r[4]=0x2001;m_r[5]=0x2000;}
  if(which==4){m_r[4]=0x12001;m_r[5]=0x2000;}
  if(which==5){m_r[4]=0xa0004000;m_r[5]=0xa0002000;}
  compare(budgets[b],true);++cases;
 }
 // READ and FETCH can map different backing memory. DT must see READ.
 check_write_page=true;
 for(int copy=0;copy<2;++copy)for(int probe=0;probe<3;++probe)for(unsigned b=0;b<18;++b) {
  byte_setup(copy,0);Sh3SetJitEnabled(b&1);memcpy(write_page,memory,sizeof(memory));
  if(probe==0){MemMapR[0]=write_page;((UINT16*)write_page)[(0x100+(copy?6:4))/2]=0x8bfd;}
  if(probe==1){Sh3MapHandler(1,0,65535,MAP_READ);Sh3SetReadWordHandler(1,dt_read);Sh3SetReadByteHandler(1,byte_io_read);dt_peek_value=0x0009;}
  if(probe==2){MemMapR[0]=write_page;MemMapW[1]=write_page;m_r[4]=0x10103+(copy?2:0);}
  const unsigned long long before=sh3_byte_loop_calls;
  compare(budgets[b],true);++cases;
  if(probe<2)CHECK(before==sh3_byte_loop_calls);
 }
 check_write_page=false;
 for(int copy=0;copy<2;++copy)for(int io=0;io<2;++io)for(unsigned b=0;b<18;++b) {
  byte_setup(copy,0);Sh3SetJitEnabled(b&1);
  Sh3MapHandler(1,0x30000,0x3ffff,MAP_READ|MAP_WRITE);
  Sh3SetReadByteHandler(1,byte_io_read);Sh3SetWriteByteHandler(1,byte_io_write);
  if(io==0)m_r[4]=0x30000;else m_r[5]=0x30000;
  compare(budgets[b],true);++cases;
 }

 // At a taken branch, rejected guards must leave every scanned field and
 // memory byte untouched. Include operand aliases and code/probe aliases.
 for(int copy=0;copy<2;++copy)for(int kind=0;kind<13;++kind) {
  byte_setup(copy,0);const unsigned branch=0x100+(copy?6:4);
  m_pc=branch+2;m_ppc=m_pc;m_sh4_icount=256;
  UINT16 *code=(UINT16*)(memory+0x100);
  if(kind==0)m_sr|=T;
  if(kind==1)m_sh4_icount=7;
  if(kind==2)m_pc=0;
  if(kind==3)code[copy?1:0]=0x2660; // dst=value=counter
  if(kind==4)code[copy?1:0]=0x2460; // value=counter
  if(kind==5)code[copy?1:0]=0x2440; // value=dst
  if(kind==6)code[copy?4:3]=0x7501; // increment wrong register
  if(kind==7){if(copy)code[0]=0x6055;else code[0]=0x2451;}
  if(kind==8)m_r[4]=0x103; // upcoming writes overlap fetched code
  if(kind==9)m_r[4]=0xe0001000;
  if(kind==10)MemMapW[0]=(UINT8*)1;
  if(kind==11)MemMapR[0]=(UINT8*)1;
  if(kind==12){memcpy(write_page,memory,sizeof(memory));MemMapR[0]=write_page;
   ((UINT16*)write_page)[branch/2]=0x8bfd;}
  const std::vector<UINT8> before=save();memcpy(saved_memory,memory,sizeof(memory));
  CHECK(!sh3_byte_loop(copy?0x8ffb:0x8ffc));CHECK(before==save());CHECK(!memcmp(saved_memory,memory,sizeof(memory)));++cases;
 }
 // A timer or pending IRQ uses its existing execution boundary, never an
 // artificial byte-count budget. Include the exact taken/terminal edges.
 for(int copy=0;copy<2;++copy)for(int kind=0;kind<3;++kind)for(int b=0;b<=130;++b) {
  byte_setup(copy,0);Sh3SetJitEnabled(b&1);m_r[6]=17;
  if(kind==0){sh4_exception_request(SH4_INTC_IRL2);m_test_irq=1;}
  if(kind==1){m_timer[0].config(0,callback);m_timer[0].start(2,0,1,1);m_timer[0].timer_prescaler=ratio_multi;}
  compare(b,true);++cases;
 }
 byte_setup(true,0);public_dispatch=false;Sh3SetJitEnabled(1);Sh3X64::release();
 printf("PASS byte-loop budgets/parity/counts/registers/delays/pages/maps/probe/callback cases=%u fast=%llu bytes=%llu\n",cases,sh3_byte_loop_calls,sh3_byte_loop_bytes);
}

int main() {
 opcode_validation_cases();
 Sh3Init(0,102400000,0,0,0,0,0,1,0,1,0);
 CHECK(!Sh3X64::enabled);
 Sh3SetJitEnabled(1);
 // Mirror a bounded backing store across the guest map, so arbitrary branch
 // targets and memory operands remain valid without installing fake handlers.
 for(unsigned i=0;i<SH3_PAGE_COUNT;++i) MemMapR[i]=MemMapW[i]=MemMapF[i]=memory;
 if(getenv("FBNEO_RAM_RANDOM_ONLY")){ram_run_random_cases();Sh3Exit();return 0;}
 byte_loop_cases();
 if(getenv("FBNEO_BYTE_ONLY")){Sh3Exit();return 0;}
 div1_oracle_cases();
 if(getenv("FBNEO_DIV1_ONLY")){Sh3Exit();return 0;}
 runtime_option_cases();
 dt_cases();
 cold_guard_cases();
 multiply_cases();
 native_dispatch_loop_cases();
 arena_capacity_cases();
 literal_cases();
 if(getenv("FBNEO_SH3_LITERAL_ONLY")) { Sh3Exit();return 0; }
 const UINT16 safe[]={0x0127,0x011a,0x411a,0x410a,0x0009,0x0018,0x0008,0xe123,0x7201,0x6123,0x312c,
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
 ram_run_cases();
 ram_run_random_cases();
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
   compare(handler==MULL?33:32,true); CHECK(Sh3X64::native_blocks>prior);
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
