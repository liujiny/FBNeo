// SH3 integer blocks -> SysV AMD64 leaf functions. No C++ handler calls are
// hidden in emitted code. RAM accesses and conditional exits are guarded;
// timers, devices and non-ALU delay slots stay with the interpreter. Derived
// code/metadata never enter the save state.
#ifndef FBNEO_SH3_X64_JIT_H
#define FBNEO_SH3_X64_JIT_H
#include <sys/mman.h>
#include <unistd.h>

namespace Sh3X64 {
#ifndef FBNEO_SH3_JIT_MIN_OPS
#define FBNEO_SH3_JIT_MIN_OPS 8
#endif
enum { SLOTS = 32768, WAYS = 4, CACHE_SETS = SLOTS/WAYS, SLOT_BYTES = 8192,
       MAX_OPS = 32, MIN_OPS = FBNEO_SH3_JIT_MIN_OPS, CODE_BYTES = 4*1024*1024, REGS = 6 };
typedef unsigned (*Entry)(UINT32 *, UINT32 *, UINT8 **, UINT8 **);
struct Block {
	UINT32 pc;
	const UINT16 *source;
	UINT16 original[MAX_OPS + 1];
	unsigned words, checked, extra_cycles;
	Entry entry;
};
static Block *blocks;
static unsigned char *code;
static size_t page_bytes;
static unsigned used, next_way[CACHE_SETS];
static bool failed, enabled = true;
#ifdef FBNEO_SH3_JIT_TEST
static unsigned long long native_blocks, native_ops, builds, lookups, misses, short_blocks;
static bool fail_allocation, fail_protection;
#endif
#ifdef FBNEO_SH3_JIT_PROFILE
static unsigned long long fallback_ops[65536];
#endif

static void release() {
#ifdef FBNEO_SH3_JIT_PROFILE
	printf("SH3JIT lookups=%llu misses=%llu short=%llu builds=%llu blocks=%llu ops=%llu\n",lookups,misses,short_blocks,builds,native_blocks,native_ops);
	for(int rank=0;rank<24;++rank) {
		unsigned best=0;
		for(unsigned op=1;op<65536;++op) if(fallback_ops[op]>fallback_ops[best]) best=op;
		if(!fallback_ops[best]) break;
		printf("SH3JIT fallback %04x %llu\n",best,fallback_ops[best]);fallback_ops[best]=0;
	}
#endif
	if (code) munmap(code, CODE_BYTES);
	free(blocks); blocks = NULL; code = NULL; failed = false; used=0;
}
static bool allocate() {
	if (failed) return false;
	if (blocks) return true;
#ifdef FBNEO_SH3_JIT_TEST
	if (fail_allocation) { failed = true; return false; }
#endif
	const long page = sysconf(_SC_PAGESIZE);
	if (page <= 0 || (page & (page - 1)) || CODE_BYTES % page) { failed = true; return false; }
	page_bytes = (size_t)page;
	blocks = (Block*)calloc(SLOTS, sizeof(Block));
	void *p = mmap(NULL, CODE_BYTES, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (!blocks || p == MAP_FAILED) {
		if (p != MAP_FAILED) munmap(p, CODE_BYTES);
		free(blocks); blocks = NULL; failed = true; return false;
	}
	code = (unsigned char*)p;
	memset(next_way,0,sizeof(next_way));
	return true;
}

// rdi = guest GPR array, rsi = SR; r8-r11, edx, ecx cache six guest GPRs.
// eax is scratch. rbx/r12/r13/r14 are preserved for guarded memory accesses.
// Generated functions never call a handler or leave callee-saved state dirty.
struct Compiler {
	unsigned char bytes[SLOT_BYTES];
	unsigned size, ops, extra_cycles;
	UINT32 pc;
	const UINT16 *source;
	int guest[REGS], age[REGS], clock;
	bool dirty[REGS], locked[REGS];
	Compiler(UINT32 address=0, const UINT16 *fetch=NULL) : size(0), ops(0), extra_cycles(0), pc(address), source(fetch), clock(0) {
		for (int i=0;i<REGS;++i) { guest[i]=-1; age[i]=0; dirty[i]=locked[i]=false; }
		byte(0x53); byte(0x41); byte(0x54); byte(0x41); byte(0x55); // preserve rbx,r12,r13
		byte(0x49); byte(0x89); byte(0xd4); // r12=read map (third SysV argument)
		byte(0x41); byte(0x56); // preserve r14
		byte(0x49); byte(0x89); byte(0xce); // r14=write map (fourth argument)
	}
	static int host(int i) { return i < 4 ? 8+i : i == 4 ? 2 : 1; }
	void byte(unsigned b) { if (size < SLOT_BYTES) bytes[size]=b; ++size; }
	void word(UINT32 v) { for (int i=0;i<4;++i) byte(v>>(i*8)); }
	void rex(int r, int b) { if (r>=8 || b>=8) byte(0x40 | ((r>>3)<<2) | (b>>3)); }
	void rr(unsigned op, int dst, int src) { rex(src,dst); byte(op); byte(0xc0|((src&7)<<3)|(dst&7)); }
	void imm(int dst, UINT32 v) { rex(0,dst); byte(0xb8+(dst&7)); word(v); }
	void memory(bool store, int reg, int base, int disp) {
		rex(reg,base); byte(store?0x89:0x8b); byte(0x40|((reg&7)<<3)|base); byte(disp);
	}
	void immediate(int ext, int dst, UINT32 v) {
		rex(0,dst); byte(0x81); byte(0xc0|(ext<<3)|(dst&7)); word(v);
	}
	void shift(int ext, int dst, int n) {
		rex(0,dst); byte(0xc1); byte(0xc0|(ext<<3)|(dst&7)); byte(n);
	}
	void unary(int ext, int dst) { rex(0,dst); byte(0xf7); byte(0xc0|(ext<<3)|(dst&7)); }
	void condition(int cc) {
		byte(0x0f); byte(0x90+cc); byte(0xc0); // setcc al
		byte(0x0f); byte(0xb6); byte(0xc0); // movzx eax,al
		byte(0x83); byte(0x26); byte(0xfe); // and dword [rsi],~T
		byte(0x09); byte(0x06); // or [rsi],eax
	}
	int reg(int g, bool read=true) {
		int pick=-1;
		for (int i=0;i<REGS;++i) {
			if (guest[i]==g) { pick=i; break; }
			if (!locked[i] && (pick<0 || guest[i]<0 || age[i]<age[pick])) pick=i;
		}
		if (guest[pick]!=g) {
			if (dirty[pick]) memory(true,host(pick),7,guest[pick]*4);
			guest[pick]=g; dirty[pick]=false;
			if (read) memory(false,host(pick),7,g*4);
		}
		age[pick]=++clock; locked[pick]=true; return host(pick);
	}
	void changed(int h) { for(int i=0;i<REGS;++i) if(host(i)==h) dirty[i]=true; }
	unsigned jump(int cc) {
		if(cc<0) byte(0xe9); else {byte(0x0f);byte(0x80+cc);}
		unsigned at=size;word(0);return at;
	}
	void patch(unsigned at) {
		UINT32 disp=size-at-4;
		if(at+4<=SLOT_BYTES) for(int i=0;i<4;++i) bytes[at+i]=disp>>(i*8);
	}
	void absolute(int h, const void *p) {
		byte(0x48|(h>>3)); byte(0xb8+(h&7));
		uintptr_t v=(uintptr_t)p; word((UINT32)v); word((UINT32)(v>>32));
	}
	void set_global(UINT32 *p, UINT32 v) {
		absolute(0,p); byte(0xc7); byte(0x00); word(v);
	}
	void finish(unsigned result=0xffffffff) {
		for (int i=0;i<REGS;++i) if(dirty[i]) memory(true,host(i),7,guest[i]*4);
		imm(0,result==0xffffffff?ops:result);
		byte(0x41);byte(0x5e);
		byte(0x41);byte(0x5d);byte(0x41);byte(0x5c);byte(0x5b);
		byte(0xc3);
	}
	bool store(UINT16 opcode, Sh3OpcodeHandler h) {
		const bool pre=h==MOVBM || h==MOVWM || h==MOVLM;
		const bool indexed=h==MOVBS0 || h==MOVWS0 || h==MOVLS0;
		const bool displaced=h==MOVBS4 || h==MOVWS4 || h==MOVLS4;
		const bool small=h==MOVBS4 || h==MOVWS4;
		const int width=h==MOVBS || h==MOVBM || h==MOVBS0 || h==MOVBS4?1:
			h==MOVWS || h==MOVWM || h==MOVWS0 || h==MOVWS4?2:4;
		int n=small?(opcode>>4)&15:(opcode>>8)&15, m=small?0:(opcode>>4)&15;
		int address=reg(n); rr(0x89,13,address);
		if(pre) immediate(5,13,width);
		if(indexed) rr(0x01,13,reg(0));
		if(displaced) immediate(0,13,(opcode&15)*width);
		unsigned exits[4],count=0;
		immediate(7,13,0xe0000000); exits[count++]=jump(3);
		if(width>1) {rex(0,13);byte(0xf7);byte(0xc5);word(width-1);exits[count++]=jump(5);}
		rr(0x89,0,13);immediate(4,0,AM);shift(5,0,SH3_SHIFT);
		byte(0x49);byte(0x8b);byte(0x1c);byte(0xc6); // rbx=[r14+rax*8]
		byte(0x48);byte(0x83);byte(0xfb);byte(SH3_MAXHANDLER);exits[count++]=jump(2);
		rr(0x89,0,13);immediate(4,0,SH3_PAGEM);
		if(width==1) immediate(6,0,1);
		byte(0x48);byte(0x01);byte(0xc3); // rbx = mapped host destination
		// Guard the actual host address, not just the guest address: another
		// physical page may alias this fetch page. Exit BEFORE any write or
		// predecrement if the write could change this region's instructions.
		absolute(0,(const void*)((uintptr_t)source-(width-1)));
		byte(0x48);byte(0x29);byte(0xc3);
		byte(0x48);byte(0x81);byte(0xfb);word(MAX_OPS*2+width-1);
		exits[count++]=jump(2);
		byte(0x48);byte(0x01);byte(0xc3);
		unsigned ready=jump(-1);
		for(unsigned i=0;i<count;++i) patch(exits[i]);
		finish(); patch(ready);
		int value=reg(m);rr(0x89,0,value);
		if(width==4) shift(0,0,16);
		if(width==2) byte(0x66);
		byte(width==1?0x88:0x89);byte(0x03); // [rbx]=al/ax/eax
		if(pre) {immediate(5,address,width);changed(address);}
		else {absolute(0,&m_ea);byte(0x44);byte(0x89);byte(0x28);}
		return true;
	}
	bool branch(UINT16 opcode, Sh3OpcodeHandler h) {
		// Not-taken conditional branches continue in this region. Taken exits
		// go through the validating dispatcher; no link bypasses code checks.
		const bool delayed=h==BTS || h==BFS;
		byte(0xf6);byte(0x06);byte(1);
		unsigned untaken=jump(h==BT || h==BTS?4:5);
		UINT32 next=pc+ops*2+2;
		UINT32 target=next+2+(INT32)(INT8)opcode*2;
		bool native_delay=false;
		if(delayed && source && ops+1<MAX_OPS && (next&SH3_PAGEM)>=2) {
			// Only pure GPR/SR operations may be fused into a taken delay slot.
			// Loads, stores, PC-relative operations and branches still exit.
			Compiler saved=*this;
			if(alu(source[ops+1])) {
				native_delay=true;
				set_global(&m_pc,target);set_global(&m_ppc,target);set_global(&m_ea,target);
				finish(0x80010000|(ops+2));
			} else size=saved.size;
			clock=saved.clock;
			for(int i=0;i<REGS;++i) {
				guest[i]=saved.guest[i];age[i]=saved.age[i];dirty[i]=saved.dirty[i];locked[i]=saved.locked[i];
			}
		}
		if(!native_delay) {
			set_global(&m_pc,target);set_global(&m_ppc,next);set_global(&m_ea,target);
			if(delayed) set_global(&m_delay,next);
			finish((delayed?0x80010000:0x80020000)|(ops+1));
		}
		patch(untaken);
		extra_cycles=2;return true;
	}
	bool load(UINT16 opcode, Sh3OpcodeHandler h) {
		const bool post=h==MOVBP || h==MOVWP || h==MOVLP;
		const bool pc_relative=h==MOVWI || h==MOVLI;
		const bool indexed=h==MOVBL0 || h==MOVWL0 || h==MOVLL0;
		const bool displaced=h==MOVBL4 || h==MOVWL4 || h==MOVLL4;
		const bool small_displaced=h==MOVBL4 || h==MOVWL4;
		const int width=h==MOVBL || h==MOVBP || h==MOVBL0 || h==MOVBL4?1:
			h==MOVWL || h==MOVWP || h==MOVWI || h==MOVWL0 || h==MOVWL4?2:4;
		const int n=small_displaced?0:(opcode>>8)&15, m=(opcode>>4)&15;
		int s=-1;
		if(pc_relative) imm(13,h==MOVWI?pc+ops*2+4+(opcode&255)*2:((pc+ops*2+4)&~3)+(opcode&255)*4);
		else {
			s=reg(m);rr(0x89,13,s);
			if(indexed) rr(0x01,13,reg(0));
			if(displaced) immediate(0,13,(opcode&15)*width);
		}
		unsigned exits[3], count=0;
		// Special/internal addresses and unaligned operands use the original
		// handler. No mapped host pointer is embedded in a generated block.
		immediate(7,13,0xe0000000); exits[count++]=jump(3);
		if(width>1) {rex(0,13);byte(0xf7);byte(0xc5);word(width-1);exits[count++]=jump(5);}
		rr(0x89,0,13);immediate(4,0,AM);shift(5,0,SH3_SHIFT);
		byte(0x49);byte(0x8b);byte(0x1c);byte(0xc4); // rbx=[r12+rax*8]
		byte(0x48);byte(0x83);byte(0xfb);byte(SH3_MAXHANDLER);exits[count++]=jump(2);
		unsigned ready=jump(-1);
		for(unsigned i=0;i<count;++i) patch(exits[i]);
		finish(); // precise partial-block exit before the faulting guest op
		patch(ready);
		rr(0x89,0,13);immediate(4,0,SH3_PAGEM);
		if(width==1) immediate(6,0,1); // guest byte addressing is word-swapped
		int d=reg(n,n==m && !pc_relative);
		rex(d,3);
		if(width<4) {byte(0x0f);byte(width==1?0xbe:0xbf);} else byte(0x8b);
		byte(((d&7)<<3)|4);byte(3); // value=[rbx+rax]
		if(width==4) shift(0,d,16);
		changed(d);
		if(post && n!=m) {immediate(0,s,width);changed(s);}
		if(!post) {
			byte(0x48);byte(0xb8);uintptr_t ea=(uintptr_t)&m_ea;
			word((UINT32)ea);word((UINT32)(ea>>32));
			byte(0x44);byte(0x89);byte(0x28); // [m_ea]=r13d
		}
		return true;
	}
	bool op(UINT16 opcode) {
		for(int i=0;i<REGS;++i) locked[i]=false;
		const Sh3OpcodeHandler h=opcode_dispatch[opcode];
		if(h==BT || h==BF || h==BTS || h==BFS) return branch(opcode,h);
		if(h==MOVBS || h==MOVWS || h==MOVLS || h==MOVBM || h==MOVWM || h==MOVLM || h==MOVBS0 || h==MOVWS0 || h==MOVLS0 || h==MOVBS4 || h==MOVWS4 || h==MOVLS4)
			return store(opcode,h);
		if(h==MOVBL || h==MOVWL || h==MOVLL || h==MOVBP || h==MOVWP || h==MOVLP || h==MOVWI || h==MOVLI || h==MOVBL0 || h==MOVWL0 || h==MOVLL0 || h==MOVBL4 || h==MOVWL4 || h==MOVLL4)
			return load(opcode,h);
		return alu(opcode);
	}
	bool alu(UINT16 opcode) {
		for(int i=0;i<REGS;++i) locked[i]=false;
		const Sh3OpcodeHandler h=opcode_dispatch[opcode];
		const int n=(opcode>>8)&15, m=(opcode>>4)&15;
		if(h==NOP) return true;
		if(h==CLRT || h==SETT) { byte(0x83); byte(h==CLRT?0x26:0x0e); byte(h==CLRT?0xfe:1); return true; }
		if(h==MOVI) { int d=reg(n,false); imm(d,(UINT32)(INT32)(INT8)opcode); changed(d); return true; }
		if(h==MOVT) {int d=reg(n,false);memory(false,d,6,0);immediate(4,d,1);changed(d);return true;}
		if(h==ADDC || h==SUBC || h==ADDV || h==SUBV) {
			int s=reg(m),d=reg(n);
			if(h==ADDC || h==SUBC) {byte(0x0f);byte(0xba);byte(0x26);byte(0);}
			rr(h==ADDC?0x11:h==SUBC?0x19:h==ADDV?0x01:0x29,d,s);
			condition(h==ADDC || h==SUBC?2:0);changed(d);return true;
		}
		if(h==NEGC) {
			int s=reg(m),d=reg(n,n==m);imm(0,0);
			byte(0x0f);byte(0xba);byte(0x26);byte(0);
			rr(0x19,0,s);rr(0x89,d,0);condition(2);changed(d);return true;
		}
		if(h==ROTCL || h==ROTCR) {
			int d=reg(n);byte(0x0f);byte(0xba);byte(0x26);byte(0);
			shift(h==ROTCL?2:3,d,1);condition(2);changed(d);return true;
		}
		if(h==XTRCT) {
			int s=reg(m),d=reg(n);rr(0x89,0,s);shift(4,0,16);shift(5,d,16);rr(0x09,d,0);changed(d);return true;
		}
		if(h==CMPSTR) {
			int s=reg(m),d=reg(n);rr(0x89,0,s);rr(0x31,0,d);
			rr(0x89,3,0);unary(2,3);immediate(5,0,0x01010101);rr(0x21,0,3);
			immediate(4,0,0x80808080);condition(5);return true;
		}
		if(h==SHAD || h==SHLD) {
			int s=reg(m),d=reg(n);rr(0x89,0,s);rr(0x89,3,d);byte(0x51);rr(0x89,1,0);
			immediate(7,1,0);unsigned negative=jump(12);
			byte(0xd3);byte(0xe3);unsigned done_positive=jump(-1);
			patch(negative);immediate(4,1,31);unsigned nonzero=jump(5);
			if(h==SHAD) shift(7,3,31);else imm(3,0);
			unsigned done_zero=jump(-1);
			patch(nonzero);unary(3,1);byte(0xd3);byte(h==SHAD?0xfb:0xeb);
			patch(done_positive);patch(done_zero);byte(0x59);rr(0x89,d,3);changed(d);return true;
		}
		if(h==MOV || h==NOT || h==NEG || h==EXTUB || h==EXTUW || h==EXTSB || h==EXTSW || h==SWAPB || h==SWAPW) {
			int s=reg(m), d=reg(n,n==m); rr(0x89,d,s);
			if(h==NOT) unary(2,d);
			if(h==NEG) unary(3,d);
			if(h==EXTUB || h==EXTUW) immediate(4,d,h==EXTUB?255:65535);
			if(h==EXTSB || h==EXTSW) { shift(4,d,h==EXTSB?24:16); shift(7,d,h==EXTSB?24:16); }
			if(h==SWAPW) shift(0,d,16);
			if(h==SWAPB) { rr(0x89,0,d); immediate(4,d,0xffff0000); byte(0x66); byte(0xc1); byte(0xc0); byte(8); immediate(4,0,65535); rr(0x09,d,0); }
			changed(d); return true;
		}
		if(h==ADDI || h==ANDI || h==XORI) {
			int d=reg(h==ADDI?n:0); immediate(h==ADDI?0:h==ANDI?4:6,d,h==ADDI?(UINT32)(INT32)(INT8)opcode:opcode&255); changed(d); return true;
		}
		if(h==ADD || h==SUB || h==AND || h==OR || h==XOR) {
			int s=reg(m),d=reg(n); rr(h==ADD?0x01:h==SUB?0x29:h==AND?0x21:h==OR?0x09:0x31,d,s); changed(d); return true;
		}
		if(h==CMPEQ || h==CMPGE || h==CMPGT || h==CMPHI || h==CMPHS || h==TST) {
			int s=reg(m),d=reg(n); rr(h==TST?0x85:0x39,d,s);
			condition(h==CMPGE?13:h==CMPGT?15:h==CMPHI?7:h==CMPHS?3:4); return true;
		}
		if(h==CMPIM || h==TSTI || h==CMPPL || h==CMPPZ) {
			int d=reg(h==CMPIM || h==TSTI?0:n);
			if(h==TSTI) { rex(0,d); byte(0xf7); byte(0xc0|(d&7)); word(opcode&255); }
			else immediate(7,d,h==CMPIM?(UINT32)(INT32)(INT8)opcode:0);
			condition(h==CMPPL?15:h==CMPPZ?13:4); return true;
		}
		if(h==SHLL || h==SHAL || h==SHLR || h==SHAR || h==ROTL || h==ROTR || h==SHLL2 || h==SHLL8 || h==SHLL16 || h==SHLR2 || h==SHLR8 || h==SHLR16) {
			int d=reg(n);
			const int amount=h==SHLL2 || h==SHLR2?2:h==SHLL8 || h==SHLR8?8:h==SHLL16 || h==SHLR16?16:1;
			shift(h==ROTL?0:h==ROTR?1:h==SHAR?7:h==SHLR || h==SHLR2 || h==SHLR8 || h==SHLR16?5:4,d,amount);
			if(amount==1) condition(2);
			changed(d); return true;
		}
		return false;
	}
};

static void compile(Block &b, UINT32 pc, const UINT16 *source) {
	if(used+SLOT_BYTES>CODE_BYTES) { memset(blocks,0,SLOTS*sizeof(Block)); used=0; }
	b.entry=NULL; b.pc=pc; b.source=source; b.words=b.checked=b.extra_cycles=0;
	Compiler c(pc,source);
	const unsigned remaining=(SH3_PAGEM+1-((pc&AM)&SH3_PAGEM))/2;
	for(unsigned i=0;i<MAX_OPS && i<remaining;++i) {
		b.original[i]=source[i]; ++b.checked;
		if(!c.op(source[i])) break;
		++b.words; ++c.ops;
	}
	b.extra_cycles=c.extra_cycles;
	if(b.words<MIN_OPS) {
#ifdef FBNEO_SH3_JIT_TEST
		++short_blocks;
#endif
		return;
	}
	c.finish(); if(c.size>SLOT_BYTES) return;
	unsigned char *dest=code+used;
	void *page=(void*)((uintptr_t)dest & ~(uintptr_t)(page_bytes-1));
	const size_t span=(((uintptr_t)dest+c.size+page_bytes-1)&~(uintptr_t)(page_bytes-1))-(uintptr_t)page;
#ifdef FBNEO_SH3_JIT_TEST
	++builds;
	if(fail_protection) { failed=true; return; }
#endif
	// Only the emulation owner compiles/runs blocks, so no page being changed
	// here can be executing. Never execute a writable mapping.
	if(mprotect(page,span,PROT_READ|PROT_WRITE)) { failed=true; return; }
	memcpy(dest,c.bytes,c.size);
	if(mprotect(page,span,PROT_READ|PROT_EXEC)) { failed=true; return; }
	b.entry=(Entry)dest;
	used+=(c.size+15)&~15;
}
} // namespace Sh3X64

static bool sh3_x64_run() {
	using namespace Sh3X64;
	if(!enabled || m_sh4_icount<MIN_OPS || m_delay || m_test_irq || !allocate()) return false;
	const UINT32 phys=m_pc&AM;
	const UINT8 *page=MemMapF[phys>>SH3_SHIFT];
	if((uintptr_t)page<SH3_MAXHANDLER || (phys&1)) return false;
	const UINT16 *source=(const UINT16*)(page+(phys&SH3_PAGEM));
	const unsigned set=((m_pc>>1)^(m_pc>>12)^(m_pc>>21))&(CACHE_SETS-1);
	unsigned way=0;
	for(;way<WAYS;++way) {
		const Block &candidate=blocks[set*WAYS+way];
		if(candidate.pc==m_pc && candidate.source==source) break;
	}
	if(way==WAYS) way=next_way[set]++&(WAYS-1);
	const unsigned slot=set*WAYS+way;
	Block &b=blocks[slot];
#ifdef FBNEO_SH3_JIT_TEST
	++lookups;
#endif
	// A negative cache entry always runs the live interpreter, so retaining
	// one after a guest edit is safe (only a missed optimization). Positive
	// entries must revalidate on EVERY call. No stale native code can execute.
	if(b.pc==m_pc && b.source==source && b.checked && !b.entry) {
#ifdef FBNEO_SH3_JIT_PROFILE
		++fallback_ops[source[0]];
#endif
		return false;
	}
	// Revalidate every native entry.
	// This covers aliased code, DMA, cheats and state loads without requiring
	// all guest-memory writers to participate in an invalidation protocol.
	if(b.pc!=m_pc || b.source!=source || !b.checked || memcmp(b.original,source,b.checked*2)) {
#ifdef FBNEO_SH3_JIT_TEST
		++misses;
#endif
		compile(b,m_pc,source);
	}
	if(failed || !b.entry || m_sh4_icount<(int)(b.words+b.extra_cycles)) {
#ifdef FBNEO_SH3_JIT_PROFILE
		++fallback_ops[source[0]];
#endif
		return false;
	}
	const unsigned result=b.entry(m_r,&m_sr,MemMapR,MemMapW);
	const unsigned completed=result&65535;
	if(!completed) {
#ifdef FBNEO_SH3_JIT_PROFILE
		++fallback_ops[source[0]];
#endif
		return false;
	}
	if(!(result&0x80000000)) {m_pc += completed*2; m_ppc=m_pc;}
	EAT(completed+((result>>16)&255));
#ifdef FBNEO_SH3_JIT_TEST
	++native_blocks; native_ops+=completed;
#endif
	return true;
}
static void sh3_x64_exit() { Sh3X64::release(); }
#endif
