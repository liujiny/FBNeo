// Direct-threaded SH3 interpreter. The original handlers and instruction
// boundary ordering are shared with the reference loop. No guest code cache,
// writable executable memory, CPU clock change or timer batching is involved.
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


template<bool SliceTimers>
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
#define SH3_FETCH() do { \
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
#define SH3_EXECUTE(name) op_##name: name(opcode); SH3_NEXT();
	SH3_THREADED_OPS(SH3_EXECUTE)
#undef SH3_EXECUTE
fallback:
	execute_one(opcode);
	SH3_NEXT();
finished:
	cycles -= m_sh4_icount;
	if (SliceTimers) sh4_run_timers(cycles);
	m_sh4_icount = 0;
	return cycles;
#undef SH3_FETCH
#undef SH3_NEXT
}

static void init_threaded_dispatch(void)
{
	Sh3Run_threaded<false>(0, true);
	Sh3Run_threaded<true>(0, true);
}
#undef SH3_THREADED_OPS
#endif
