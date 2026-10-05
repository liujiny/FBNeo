// Direct-threaded SH3 interpreter. The original handlers and instruction
// boundary ordering are shared with the reference loop. The default path has
// no guest code cache. FBNEO_SH3_X64_JIT optionally adds guarded native blocks;
// CPU clocks and timer boundaries are unchanged.
// GNU labels-as-values keep common integer handlers inside the dispatch loop;
// uncommon operations call the existing function-pointer interpreter.
#ifndef FBNEO_SH3_THREADED_H
#define FBNEO_SH3_THREADED_H

#define SH3_THREADED_OPS(OP) \
	OP(ADD) \
	OP(ADDC) \
	OP(ADDI) \
	OP(ADDV) \
	OP(AND) \
	OP(ANDI) \
	OP(BF) \
	OP(BFS) \
	OP(BRA) \
	OP(BRAF) \
	OP(BSR) \
	OP(BSRF) \
	OP(BT) \
	OP(BTS) \
	OP(CLRT) \
	OP(CMPEQ) \
	OP(CMPGE) \
	OP(CMPGT) \
	OP(CMPHI) \
	OP(CMPHS) \
	OP(CMPIM) \
	OP(CMPPL) \
	OP(CMPPZ) \
	OP(CMPSTR) \
	OP(DT) \
	OP(EXTSB) \
	OP(EXTSW) \
	OP(EXTUB) \
	OP(EXTUW) \
	OP(JMP) \
	OP(JSR) \
	OP(MOV) \
	OP(MOVA) \
	OP(MOVBL) \
	OP(MOVBL0) \
	OP(MOVBL4) \
	OP(MOVBLG) \
	OP(MOVBM) \
	OP(MOVBP) \
	OP(MOVBS) \
	OP(MOVBS0) \
	OP(MOVBS4) \
	OP(MOVBSG) \
	OP(MOVI) \
	OP(MOVLI) \
	OP(MOVLL) \
	OP(MOVLL0) \
	OP(MOVLL4) \
	OP(MOVLLG) \
	OP(MOVLM) \
	OP(MOVLP) \
	OP(MOVLS) \
	OP(MOVLS0) \
	OP(MOVLS4) \
	OP(MOVLSG) \
	OP(MOVT) \
	OP(MOVWI) \
	OP(MOVWL) \
	OP(MOVWL0) \
	OP(MOVWL4) \
	OP(MOVWLG) \
	OP(MOVWM) \
	OP(MOVWP) \
	OP(MOVWS) \
	OP(MOVWS0) \
	OP(MOVWS4) \
	OP(MOVWSG) \
	OP(MULL) \
	OP(MULS) \
	OP(MULU) \
	OP(NEG) \
	OP(NEGC) \
	OP(NOP) \
	OP(NOT) \
	OP(OR) \
	OP(ORI) \
	OP(ROTCL) \
	OP(ROTCR) \
	OP(ROTL) \
	OP(ROTR) \
	OP(RTS) \
	OP(SETT) \
	OP(SHAD) \
	OP(SHAL) \
	OP(SHAR) \
	OP(SHLD) \
	OP(SHLL) \
	OP(SHLL16) \
	OP(SHLL2) \
	OP(SHLL8) \
	OP(SHLR) \
	OP(SHLR16) \
	OP(SHLR2) \
	OP(SHLR8) \
	OP(STSPR) \
	OP(SUB) \
	OP(SUBC) \
	OP(SUBV) \
	OP(SWAPB) \
	OP(SWAPW) \
	OP(TST) \
	OP(TSTI) \
	OP(XOR) \
	OP(XORI) \
	OP(XTRCT)


// Only these one-cycle register operations can defer bookkeeping. They
// never access RAM/devices, alter IRQ masking, branch or consume extra cycles.
#ifndef FBNEO_SH3_ALU_RUNS
#define FBNEO_SH3_ALU_RUNS 1
#endif
#define SH3_ALU_OPS(OP) \
	OP(ADD) \
	OP(ADDC) \
	OP(ADDI) \
	OP(ADDV) \
	OP(AND) \
	OP(ANDI) \
	OP(CLRT) \
	OP(CMPEQ) \
	OP(CMPGE) \
	OP(CMPGT) \
	OP(CMPHI) \
	OP(CMPHS) \
	OP(CMPIM) \
	OP(CMPPL) \
	OP(CMPPZ) \
	OP(CMPSTR) \
	OP(EXTSB) \
	OP(EXTSW) \
	OP(EXTUB) \
	OP(EXTUW) \
	OP(MOV) \
	OP(MOVI) \
	OP(NEG) \
	OP(NEGC) \
	OP(NOP) \
	OP(NOT) \
	OP(OR) \
	OP(ROTCL) \
	OP(ROTCR) \
	OP(ROTL) \
	OP(ROTR) \
	OP(SETT) \
	OP(SHAD) \
	OP(SHAL) \
	OP(SHAR) \
	OP(SHLD) \
	OP(SHLL) \
	OP(SHLL16) \
	OP(SHLL2) \
	OP(SHLL8) \
	OP(SHLR) \
	OP(SHLR16) \
	OP(SHLR2) \
	OP(SHLR8) \
	OP(SUB) \
	OP(SUBC) \
	OP(SUBV) \
	OP(SWAPB) \
	OP(SWAPW) \
	OP(TST) \
	OP(TSTI) \
	OP(XOR) \
	OP(XORI) \
	OP(XTRCT)

#define SH3_OTHER_OPS(OP) \
	OP(BF) \
	OP(BFS) \
	OP(BRA) \
	OP(BRAF) \
	OP(BSR) \
	OP(BSRF) \
	OP(BT) \
	OP(BTS) \
	OP(DT) \
	OP(JMP) \
	OP(JSR) \
	OP(MOVA) \
	OP(MOVBL) \
	OP(MOVBL0) \
	OP(MOVBL4) \
	OP(MOVBLG) \
	OP(MOVBM) \
	OP(MOVBP) \
	OP(MOVBS) \
	OP(MOVBS0) \
	OP(MOVBS4) \
	OP(MOVBSG) \
	OP(MOVLI) \
	OP(MOVLL) \
	OP(MOVLL0) \
	OP(MOVLL4) \
	OP(MOVLLG) \
	OP(MOVLM) \
	OP(MOVLP) \
	OP(MOVLS) \
	OP(MOVLS0) \
	OP(MOVLS4) \
	OP(MOVLSG) \
	OP(MOVT) \
	OP(MOVWI) \
	OP(MOVWL) \
	OP(MOVWL0) \
	OP(MOVWL4) \
	OP(MOVWLG) \
	OP(MOVWM) \
	OP(MOVWP) \
	OP(MOVWS) \
	OP(MOVWS0) \
	OP(MOVWS4) \
	OP(MOVWSG) \
	OP(MULL) \
	OP(MULS) \
	OP(MULU) \
	OP(ORI) \
	OP(RTS) \
	OP(STSPR)

// A direct aligned RAM access cannot invoke a device or make an IRQ
// visible. Preserve live maps/data and fall back before any guest mutation.
#ifdef FBNEO_SH3_JIT_TEST
static unsigned long long sh3_ram_run_ops;
#endif
static inline bool sh3_ram_run(UINT16 opcode, UINT32 next_pc, Sh3OpcodeHandler H) {
#ifndef WaitState
 return false; // If optional wait-state accounting becomes active, retain it.
#else
 const unsigned n=(opcode>>8)&15,m=(opcode>>4)&15;
 const bool store=H==MOVLS4 || H==MOVLM;
 UINT32 address=H==MOVLI?((next_pc+2)&~3)+(opcode&255)*4:
  H==MOVLL4?m_r[m]+(opcode&15)*4:
  H==MOVLS4?m_r[n]+(opcode&15)*4:
  H==MOVLP?m_r[m]:m_r[n]-4;
 if(address>=0xe0000000 || (address&3)) return false;
 const UINT32 phys=address&AM;
 UINT8 *page=(store?MemMapW:MemMapR)[phys>>SH3_SHIFT];
 if((uintptr_t)page<SH3_MAXHANDLER) return false;
 UINT32 *host=(UINT32*)(page+(phys&SH3_PAGEM));
#ifdef FBNEO_SH3_JIT_TEST
 ++sh3_ram_run_ops;
#endif
 if(H==MOVLL4 || H==MOVLS4 || H==MOVLI) m_ea=address;
 if(store) {
  UINT32 value=m_r[m]; // MOVLM aliases must read before decrementing Rn.
  if(H==MOVLM) m_r[n]=address;
#ifdef LSB_FIRST
  value=(value<<16)|(value>>16);
#endif
  *host=value;
 } else {
  UINT32 value=*host;
#ifdef LSB_FIRST
  value=(value<<16)|(value>>16);
#endif
  m_r[n]=value;
  if(H==MOVLP && n!=m) m_r[m]+=4;
 }
 return true;
#endif
}
// Batch two live byte-loop patterns at a taken BFS, after its prefix has
// executed normally. Preserve the pending branch/delay pair and every cycle.
#ifdef FBNEO_SH3_JIT_TEST
static unsigned long long sh3_byte_loop_calls, sh3_byte_loop_bytes;
#endif
static inline bool sh3_byte_ranges_overlap(uintptr_t a,uintptr_t ae,uintptr_t b,uintptr_t be) {
 return a<be && b<ae;
}
__attribute__((noinline))
static bool sh3_byte_loop(UINT16 opcode) {
#ifndef WaitState
 return false;
#else
 const bool copy=opcode==0x8ffb;
 if((!copy && opcode!=0x8ffc) || (m_sr&T) || !m_pc)return false;
 const unsigned iteration_cycles=copy?6:5;
 if(m_sh4_icount<(int)(3+iteration_cycles))return false;
 const UINT32 loop_pc=m_pc+2+(INT32)(INT8)opcode*2;
 const unsigned words=copy?5:4;
 const UINT32 code_phys=loop_pc&AM,code_off=code_phys&SH3_PAGEM;
 if((loop_pc&1) || code_off+words*2>SH3_PAGEM+1)return false;
 const UINT8 *fetch=MemMapF[code_phys>>SH3_SHIFT];
 if((uintptr_t)fetch<SH3_MAXHANDLER)return false;
 const UINT16 *code=(const UINT16*)(fetch+code_off);
 const unsigned store_index=copy?1:0,dt_index=copy?2:1;
 if((code[store_index]&0xf00f)!=0x2000 || (code[dt_index]&0xf0ff)!=0x4010 ||
    code[dt_index+1]!=opcode || (code[words-1]&0xf0ff)!=0x7001)return false;
 const unsigned dst=(code[store_index]>>8)&15,value=(code[store_index]>>4)&15;
 const unsigned counter=(code[dt_index]>>8)&15;
 if(((code[words-1]>>8)&15)!=dst || dst==counter || value==counter || value==dst)return false;
 unsigned src=0;
 if(copy) {
  if((code[0]&0xf00f)!=0x6004 || ((code[0]>>8)&15)!=value)return false;
  src=(code[0]>>4)&15;
  if(src==dst || src==counter || src==value)return false;
 }
 // DT reads through READ, independently of FETCH, even without taking its
 // busy-loop shortcut. A device/probe edit or 8bfd must retain that behavior.
 const UINT32 probe_pc=loop_pc+(dt_index+1)*2;
 if(probe_pc>=0xe0000000)return false;
 const UINT32 probe_phys=probe_pc&AM;
 const UINT8 *probe_page=MemMapR[probe_phys>>SH3_SHIFT];
 if((uintptr_t)probe_page<SH3_MAXHANDLER)return false;
 const UINT8 *probe=probe_page+(probe_phys&SH3_PAGEM);
 if((uintptr_t)probe&1)return false;
#if BUSY_LOOP_HACKS
 if(*(const UINT16*)probe==0x8bfd)return false;
#endif
 const UINT32 dest=m_r[dst]+1; // the current BFS delay slot increments it
 const UINT32 source=copy?m_r[src]:0;
 if(dest>=0xe0000000 || (copy && source>=0xe0000000))return false;
 const UINT32 dp=dest&AM,sp=source&AM;
 UINT8 *write=MemMapW[dp>>SH3_SHIFT];
 const UINT8 *read=copy?MemMapR[sp>>SH3_SHIFT]:NULL;
 if((uintptr_t)write<SH3_MAXHANDLER || (copy && (uintptr_t)read<SH3_MAXHANDLER))return false;
 const unsigned doff=dp&SH3_PAGEM,soff=sp&SH3_PAGEM;
 unsigned count=(m_sh4_icount-3)/iteration_cycles;
 if(m_r[counter] && count>m_r[counter])count=m_r[counter];
 if(count>SH3_PAGEM+1-doff)count=SH3_PAGEM+1-doff;
 if(copy && count>SH3_PAGEM+1-soff)count=SH3_PAGEM+1-soff;
 if(!count)return false;
 // Word-swapped bytes occupy the enclosing even-aligned host interval.
 const uintptr_t dlo=(uintptr_t)(write+(doff&~1U)),dhi=(uintptr_t)(write+((doff+count+1)&~1U));
 if(sh3_byte_ranges_overlap(dlo,dhi,(uintptr_t)code,(uintptr_t)code+words*2) ||
    sh3_byte_ranges_overlap(dlo,dhi,(uintptr_t)probe,(uintptr_t)probe+2))return false;
 if(copy) {
  const uintptr_t slo=(uintptr_t)(read+(soff&~1U)),shi=(uintptr_t)(read+((soff+count+1)&~1U));
  if(sh3_byte_ranges_overlap(dlo,dhi,slo,shi))return false;
 }
 UINT8 last=0;
#ifdef LSB_FIRST
 if(copy) {
  last=read[(soff+count-1)^1];
  if((soff^doff)&1) {
   for(unsigned i=0;i<count;++i)write[(doff+i)^1]=read[(soff+i)^1];
  } else {
   unsigned i=0;
   if(doff&1){write[doff^1]=read[soff^1];i=1;}
   const unsigned pairs=(count-i)&~1U;
   if(pairs)memcpy(write+doff+i,read+soff+i,pairs);
   i+=pairs;if(i<count)write[(doff+i)^1]=read[(soff+i)^1];
  }
 } else {
  const UINT8 byte=(UINT8)m_r[value];unsigned i=0;
  if(doff&1){write[doff^1]=byte;i=1;}
  const unsigned pairs=(count-i)&~1U;
  if(pairs)memset(write+doff+i,byte,pairs);
  i+=pairs;if(i<count)write[(doff+i)^1]=byte;
 }
#else
 if(copy){last=read[soff+count-1];memcpy(write+doff,read+soff,count);}
 else memset(write+doff,(UINT8)m_r[value],count);
#endif
 if(copy){m_r[src]+=count;m_r[value]=(UINT32)(INT32)(INT8)last;}
 m_r[dst]=dest+count;m_r[counter]-=count;
 const bool terminal=m_r[counter]==0;
 m_sr=(m_sr&~T)|(terminal?T:0);
 m_delay=0;m_pc=terminal?loop_pc+words*2:loop_pc;m_ppc=m_pc;
 m_ea=terminal?dest+count-1:loop_pc;
 const unsigned cycles=3+count*iteration_cycles-(terminal?1:0);EAT(cycles);
#ifdef FBNEO_SH3_JIT_TEST
 ++sh3_byte_loop_calls;sh3_byte_loop_bytes+=count;
#endif
 return true;
#endif
}

template<bool SliceTimers, bool UseJit>
#if defined(__GNUC__) && !defined(__clang__)
__attribute__((noinline, noclone))
#else
__attribute__((noinline))
#endif
static int Sh3Run_threaded(int cycles, bool initialize)
{
	static void *entries[65536];
	UINT16 opcode;
	INT32 timer_start = 0;
	INT32 alu_left = 0;
	UINT32 alu_pc = 0;
	bool alu_active = false;
#if FBNEO_SH3_X64_JIT
	bool jit_entry = true;
#endif
	if (initialize) {
		struct Entry { Sh3OpcodeHandler handler; void *label; };
#define SH3_ENTRY(name) { name, &&op_##name },
		static const Entry known[] = { SH3_THREADED_OPS(SH3_ENTRY) };
#undef SH3_ENTRY
		for (unsigned op = 0; op < 65536; ++op) {
			entries[op] = &&fallback;
			for (unsigned i = 0; i < sizeof(known) / sizeof(known[0]); ++i)
				if (opcode_dispatch[op] == known[i].handler) {
					entries[op] = known[i].label;
					break;
				}
		}
		return 0;
	}
	m_sh4_icount = cycles;
	sh3_end_run = 0;
	if (m_cpu_off) {
		m_sh4_icount = 0;
		sh3_total_cycles += cycles;
		return cycles;
	}
	goto fetch;

	// Commit before any operation that can observe CPU state or access a
	// device, and before returning. Opcodes still come from live guest RAM.
#define SH3_COMMIT_ALU() do { \
	if (alu_active) { \
		sh3_total_cycles += m_sh4_icount - alu_left; \
		m_sh4_icount = alu_left; \
		m_pc = m_ppc = alu_pc; \
		alu_active = false; \
	} \
} while (0)

	// Finish precisely one guest instruction before taking an interrupt,
	// charging its base cycle and advancing the timers in normal mode.
	// Macro expansion allows a separate next-op branch at each hot handler.
#define SH3_NEXT() do { \
	if (m_test_irq && !m_delay) sh4_check_pending_irq(); \
	EAT(1); \
	if (!SliceTimers) sh4_run_timers(sh3_total_cycles - timer_start); \
	if (m_sh4_icount <= 0) goto finished; \
	SH3_FETCH(); \
} while (0)
#if FBNEO_SH3_X64_JIT
#define SH3_JIT_BRANCH(name) do { \
	if (UseJit && (name==BF || name==BFS || name==BRA || name==BRAF || name==BSR || name==BSRF \
		|| name==BT || name==BTS || name==JMP || name==JSR || name==RTS)) jit_entry=true; \
} while (0)
#define SH3_JIT_ALU_ALLOWED (!UseJit || !jit_entry)
#define SH3_TRY_NATIVE() do { \
	if (UseJit && jit_entry && SliceTimers && !m_delay && !m_test_irq) { \
		jit_entry = false; \
		if (sh3_x64_run()) goto finished; \
		if (m_delay) jit_entry = true; \
	} \
} while (0)
#else
#define SH3_JIT_BRANCH(name) do {} while (0)
#define SH3_JIT_ALU_ALLOWED true
#define SH3_TRY_NATIVE() do {} while (0)
#endif
#define SH3_FETCH() do { \
	SH3_TRY_NATIVE(); \
	if (!SliceTimers) timer_start = sh3_total_cycles; \
	if (m_delay) { \
		opcode = sh3_cpu_readop16((UINT32)(m_delay & AM)); \
		m_delay = 0; \
		m_ppc = m_pc; \
	} else { \
		opcode = sh3_cpu_readop16((UINT32)(m_pc & AM)); \
		m_pc += 2; \
		m_ppc = m_pc; \
	} \
	goto *entries[opcode]; \
} while (0)

fetch:
	SH3_FETCH();
#define SH3_EXECUTE(name) op_##name: \
 if(SliceTimers && UseJit && name==BFS && !m_test_irq) { \
  SH3_COMMIT_ALU(); \
  if((opcode==0x8ffb || opcode==0x8ffc) && !(m_sr&T) && m_sh4_icount>=8 && sh3_byte_loop(opcode)) { \
   SH3_JIT_BRANCH(name); \
   if(m_sh4_icount<=0)goto finished; \
   goto fetch; \
  } \
 } \
 if(!UseJit && FBNEO_SH3_ALU_RUNS && SliceTimers && !m_test_irq \
  && (name==MOVLL4 || name==MOVLS4 || name==MOVLP || name==MOVLM || name==MOVLI) \
  && sh3_ram_run(opcode,alu_active?alu_pc:m_pc,name)) { \
  if(!alu_active) {alu_pc=m_pc;alu_left=m_sh4_icount;alu_active=true;} \
  if(--alu_left<=0) goto finished; \
  opcode=sh3_cpu_readop16(alu_pc&AM);alu_pc+=2;goto *entries[opcode]; \
 } \
 SH3_COMMIT_ALU();name(opcode);SH3_JIT_BRANCH(name);SH3_NEXT();
	SH3_OTHER_OPS(SH3_EXECUTE)
#undef SH3_EXECUTE
	// Slice timers advance only at the existing run boundary. Without a
	// pending IRQ, these one-cycle register operations cannot make an IRQ
	// visible between themselves. Memory, branches and fallback commit first.
#define SH3_EXECUTE_ALU(name) op_##name: \
	if (FBNEO_SH3_ALU_RUNS && SliceTimers && !m_test_irq && SH3_JIT_ALU_ALLOWED) { \
		if (!alu_active) { alu_pc = m_pc; alu_left = m_sh4_icount; alu_active = true; } \
		name(opcode); \
		if (--alu_left <= 0) goto finished; \
		opcode = sh3_cpu_readop16(alu_pc & AM); \
		alu_pc += 2; \
		goto *entries[opcode]; \
	} \
	SH3_COMMIT_ALU(); name(opcode); SH3_NEXT();
	SH3_ALU_OPS(SH3_EXECUTE_ALU)
#undef SH3_EXECUTE_ALU
fallback:
	SH3_COMMIT_ALU();
	execute_one(opcode);
	SH3_NEXT();
finished:
	SH3_COMMIT_ALU();
	cycles -= m_sh4_icount;
	if (SliceTimers) sh4_run_timers(cycles);
	m_sh4_icount = 0;
	return cycles;
#undef SH3_COMMIT_ALU
#undef SH3_TRY_NATIVE
#undef SH3_JIT_BRANCH
#undef SH3_JIT_ALU_ALLOWED
#undef SH3_FETCH
#undef SH3_NEXT
}

static void init_threaded_dispatch(void)
{
	Sh3Run_threaded<false, false>(0, true);
	Sh3Run_threaded<true, (FBNEO_SH3_X64_JIT != 0)>(0, true);
#if FBNEO_SH3_X64_JIT
	Sh3Run_threaded<true, false>(0, true);
#endif
}
#undef SH3_ALU_OPS
#undef SH3_OTHER_OPS
#undef SH3_THREADED_OPS
#endif
