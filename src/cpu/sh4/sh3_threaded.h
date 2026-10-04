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
		while (sh3_x64_run()) \
			if (m_sh4_icount <= 0) goto finished; \
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
#define SH3_EXECUTE(name) op_##name: SH3_COMMIT_ALU(); name(opcode); SH3_JIT_BRANCH(name); SH3_NEXT();
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
