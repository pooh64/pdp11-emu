#include "emu.h"
#include "isa.h"
#include "common.h"

#define DEF_EXECUTE(instr) static inline void		\
	Execute_##instr(word_t opcode, struct Emu &emu)

#define DEF_DISASMS(instr) static inline void		\
	Disasms_##instr(word_t opcode, struct Emu &emu, std::ostream &os)

static inline bool getSign(byte_t val)  { return 0x80 & val; }
static inline bool getSign(word_t val)  { return 0x8000 & val; }
static inline bool getSign(dword_t val) { return 0x80000000 & val; }

static inline word_t SignExtend(byte_t val)
{
	return (0x80 & val) ? 0xff00 | val : val;
}

static inline dword_t SignExtend(word_t val)
{
	return (0x8000 & val) ? 0xffff0000 | val : val;
}

static inline bool getZ(byte_t val)  { return !val; }
static inline bool getZ(word_t val)  { return !val; }
static inline bool getZ(dword_t val) { return !val; }

static inline bool DetectSubOvf(byte_t src, byte_t dst, byte_t val)
{
	val = (src ^ dst) & (~src ^ val);
	return getSign(val);
}
static inline bool DetectSubOvf(word_t src, word_t dst, word_t val)
{
	val = (src ^ dst) & (~src ^ val);
	return getSign(val);
}

static inline bool DetectAddOvf(byte_t src, byte_t dst, byte_t val)
{
	val = (~src ^ dst) & (src ^ val);
	return getSign(val);
}
static inline bool DetectAddOvf(word_t src, word_t dst, word_t val)
{
	val = (~src ^ dst) & (src ^ val);
	return getSign(val);
}

/* Addressing unit (r+m) */
struct AddrOp {
	union {
		word_t ptr;
		uint8_t reg;
	} effAddr;
	bool isReg;

	uint8_t op_mode;
	uint8_t op_reg;

	void init(uint8_t _mode, uint8_t _reg) { op_mode = _mode; op_reg = _reg; }
	void Disasm(std::ostream &os);

	template <typename T>
	void Fetch(Emu &emu);
	template <typename T>
	void Load(Emu &emu, T *val);
	template <typename T>
	void Store(Emu &emu, T val);
};

struct InstrOp_mrmr {
	union Format {
		struct {
			unsigned dr : 3;
			unsigned dm : 3;
			unsigned sr : 3;
			unsigned sm : 3;
			unsigned op : 4;
		};
		word_t raw;
	};
	AddrOp s, d;
	InstrOp_mrmr(word_t raw) {
		Format fmt; fmt.raw = raw;
		s.init(fmt.sm, fmt.sr);
		d.init(fmt.dm, fmt.dr);
	}
	template<typename T>
	void Fetch(Emu &emu) { s.Fetch<T>(emu); d.Fetch<T>(emu); }

	void Disasm(std::ostream &os) {
		os << " ";  s.Disasm(os);
		os << ", "; d.Disasm(os);
	}
#define PREF_MRMR_W InstrOp_mrmr op(opcode); op.Fetch<word_t>(emu); \
	word_t val, src, dst; (void) val; (void) src; (void) dst;
#define PREF_MRMR_B InstrOp_mrmr op(opcode); op.Fetch<byte_t>(emu); \
	byte_t val, src, dst; (void) val; (void) src; (void) dst;
};

struct InstrOp_rmr {
	union Format {
		struct {
			unsigned ar : 3;
			unsigned am : 3;
			unsigned r  : 3;
			unsigned op : 7;
		};
		word_t raw;
	};
	AddrOp r, a;
	InstrOp_rmr(word_t raw) {
		Format fmt; fmt.raw = raw;
		r.init(0, fmt.r);
		a.init(fmt.am, fmt.ar);
	}
	void Fetch(Emu &emu) { r.Fetch<word_t>(emu); a.Fetch<word_t>(emu); }

	void DisasmRSS(std::ostream &os) {
		os << " ";  a.Disasm(os);
		os << ", "; r.Disasm(os);
	}

	void DisasmRDD(std::ostream &os) {
		os << " ";  r.Disasm(os);
		os << ", "; a.Disasm(os);
	}
#define PREF_RMR InstrOp_rmr op(opcode); op.Fetch(emu);
};

struct InstrOp_mr {
	union Format {
		struct {
			unsigned ar : 3;
			unsigned am : 3;
			unsigned op : 10;
		};
		word_t raw;
	};
	AddrOp a;
	InstrOp_mr(word_t raw) {
		Format fmt; fmt.raw = raw;
		a.init(fmt.am, fmt.ar);
	}
	template<typename T>
	void Fetch(Emu &emu) { a.Fetch<T>(emu); }

	void Disasm(std::ostream &os) {
		os << " ";  a.Disasm(os);
	}
#define PREF_MR_W InstrOp_mr op(opcode); op.Fetch<word_t>(emu); word_t val; (void) val;
#define PREF_MR_B InstrOp_mr op(opcode); op.Fetch<byte_t>(emu); byte_t val; (void) val;
};

struct InstrOp_r {
	union Format {
		struct {
			unsigned r : 3;
			unsigned op : 13;
		};
		word_t raw;
	};
	AddrOp r;
	InstrOp_r(word_t raw) {
		Format fmt; fmt.raw = raw;
		r.init(0, fmt.r);
	}
	void Fetch(Emu &emu) { r.Fetch<word_t>(emu); }

	void Disasm(std::ostream &os) {
		os << " ";  r.Disasm(os);
	}
#define PREF_R InstrOp_r op(opcode); op.Fetch(emu);
};

template <typename T>
void AddrOp::Fetch(Emu &emu)	// Execute unit to get effAddr, may abort
{
	word_t &pc = emu.genReg[Emu::REG_PC];
	word_t &reg = emu.genReg[op_reg];
	word_t imm;

	isReg = false;
	switch (op_mode) {
	case 0b000: // R
		isReg = true;
		effAddr.reg = op_reg;
		break;
	case 0b001: // (R)
		effAddr.ptr = reg;
		break;
	case 0b010: // (R)+
		effAddr.ptr = reg;
		reg += (op_reg < Emu::REG_SP) ? sizeof(T) : sizeof(word_t);
		// sp trap
		break;
	case 0b011: // *(R)+
		emu.Load<word_t>(reg, &effAddr.ptr);
		reg += sizeof(word_t);
		// sp trap
		break;
	case 0b100: // -(R)
		reg -= (op_reg < Emu::REG_SP) ? sizeof(T) : sizeof(word_t);
		// sp trap
		effAddr.ptr = reg;
		break;
	case 0b101: // *-(R)
		reg -= sizeof(word_t);
		// sp trap
		emu.Load<word_t>(reg, &effAddr.ptr);
		break;
	case 0b110: // imm(R)
		emu.Load<word_t>(pc, &imm);
		pc += sizeof(word_t);
		effAddr.ptr = reg + imm;
		break;
	case 0b111: // *imm(R)
		emu.Load<word_t>(pc, &imm);
		pc += sizeof(word_t);
		emu.Load<word_t>(reg + imm, &effAddr.ptr);
		break;
	}
}

template <typename T>
inline void AddrOp::Load(Emu &emu, T *val) // may abort
{
	if (isReg)
		*val = emu.genReg[effAddr.reg];
	else
		emu.Load<T>(effAddr.ptr, val);
}

template <typename T>
inline void AddrOp::Store(Emu &emu, T val) // may abort
{
	if (isReg) {
		auto ptr = &emu.genReg[effAddr.reg];
		*reinterpret_cast<T*>(ptr) = val;
	} else
		emu.Store<T>(effAddr.ptr, val);
}

void AddrOp::Disasm(std::ostream &os)
{
	switch (op_mode) {
		case 0b000: // R
			os << putf("r%d", op_reg); break;
		case 0b001: // (R)
			os << putf("(r%d)", op_reg); break;
		case 0b010: // (R)+
			os << putf("(r%d)+", op_reg); break;
		case 0b011: // *(R)+
			os << putf("*(r%d)+", op_reg); break;
		case 0b100: // -(R)
			os << putf("-(r%d)", op_reg); break;
		case 0b101: // *-(R)
			os << putf("*-(r%d)", op_reg); break;
		case 0b110: // imm(R)
			os << putf("imm(r%d)", op_reg); break;
		case 0b111: // *imm(R)
			os << putf("*imm(r%d)", op_reg); break;
	}
}

DEF_EXECUTE(unknown) { emu.RaiseTrap(Emu::TRAP_ILL); }
DEF_DISASMS(unknown) { }

/******************************* Branches *************************************/

static inline
void ExecuteBranch(struct Emu &emu, word_t raw)
{
	word_t offs = (word_t) 2 * SignExtend((byte_t) (raw & 0xff));
	emu.genReg[Emu::REG_PC] += offs;
}

#define DEF_BRANCH_LIST						\
	DEF_BRANCH(br,   true)					\
	DEF_BRANCH(beq,  emu.psw.z)				\
	DEF_BRANCH(bne, !emu.psw.z)				\
	DEF_BRANCH(bmi,  emu.psw.n)				\
	DEF_BRANCH(bpl, !emu.psw.n)				\
	DEF_BRANCH(bcs,  emu.psw.c)				\
	DEF_BRANCH(bcc, !emu.psw.c)				\
	DEF_BRANCH(bvs,  emu.psw.v)				\
	DEF_BRANCH(bvc, !emu.psw.v)				\
	DEF_BRANCH(blt,   emu.psw.n | emu.psw.v)		\
	DEF_BRANCH(bge, !(emu.psw.n | emu.psw.v))		\
	DEF_BRANCH(ble,   emu.psw.z | emu.psw.n | emu.psw.v)	\
	DEF_BRANCH(bgt, !(emu.psw.z | emu.psw.n | emu.psw.v))	\
	DEF_BRANCH(bhi, !(emu.psw.c | emu.psw.z))		\
	DEF_BRANCH(blos,  emu.psw.c | emu.psw.z)

#define DEF_BRANCH(name, pred)	\
DEF_EXECUTE(name) { if (pred) ExecuteBranch(emu, opcode); }
DEF_BRANCH_LIST
#undef DEF_BRANCH

#define DEF_BRANCH(name, pred)	\
DEF_DISASMS(name) { os << " pc+" << (opcode & 0xff); }
DEF_BRANCH_LIST
#undef DEF_BRANCH

/************************* Cond. code operators *******************************/

union __attribute__((may_alias)) CCODEop {
	struct {
		unsigned c : 1;
		unsigned v : 1;
		unsigned z : 1;
		unsigned n : 1;
		unsigned val : 1;
	};
	word_t raw;
};
static const char CCODEopStrtab[] = {'c', 'v', 'z', 'n'};

DEF_EXECUTE(ccode_op) {
	CCODEop op; op.raw = opcode;

	if (op.val)
		emu.psw.raw |=  (op.raw & 0b1111);
	else
		emu.psw.raw &= ~(op.raw & 0b1111);
}

DEF_DISASMS(ccode_op) {
	CCODEop op; op.raw = opcode;

	if (op.val)
		os << ": se";
	else
		os << ": cl";

	for (int i = 0; i < 4; ++i, op.raw >>= 1) {
		if (op.raw & 1)
			os << " " << CCODEopStrtab[i];
	}
}

/*************************** 2-op logical instrs ******************************/

#define DEF_DLOG_LIST				\
	DEF_DLOG(bis,  (src) | (dst), true)	\
	DEF_DLOG(bic, ~(src) & (dst), true)	\
	DEF_DLOG(bit,  (src) & (dst), false)	\

#define DEF_DLOG(name, expr, wback)		\
DEF_EXECUTE(name) {				\
	PREF_MRMR_W;				\
	op.s.Load(emu, &src);			\
	op.d.Load(emu, &dst);			\
	val = (expr);				\
	emu.psw.n = getSign(val);		\
	emu.psw.z = getZ(val);			\
	emu.psw.v = 0;				\
	if (wback)				\
		op.d.Store(emu, val);		\
} DEF_DISASMS(name) { }
DEF_DLOG_LIST
#undef DEF_DLOG

#define DEF_DLOG(name, expr, wback)		\
DEF_EXECUTE(name##b) {				\
	PREF_MRMR_B;				\
	op.s.Load(emu, &src);			\
	op.d.Load(emu, &dst);			\
	val = (expr);				\
	emu.psw.n = getSign(val);		\
	emu.psw.z = getZ(val);			\
	emu.psw.v = 0;				\
	if (wback)				\
		op.d.Store(emu, val);		\
} DEF_DISASMS(name##b) { }
DEF_DLOG_LIST
#undef DEF_DLOG


/********************************** Other *************************************/

DEF_EXECUTE(clr) {
	PREF_MR_W;
	emu.psw.n = emu.psw.v = emu.psw.c = 0;
	emu.psw.z = 1;
	op.a.Store(emu, (word_t) 0);
}
DEF_DISASMS(clr) { InstrOp_mr(opcode).Disasm(os); }
DEF_EXECUTE(clrb) {
	PREF_MR_B;
	emu.psw.n = emu.psw.v = emu.psw.c = 0;
	emu.psw.z = 1;
	op.a.Store(emu, (byte_t) 0);
}
DEF_DISASMS(clrb) { InstrOp_mr(opcode).Disasm(os); }

DEF_EXECUTE(dec) {
	PREF_MR_W;
	op.a.Load(emu, &val);
	val = val - 1;
	emu.psw.n = getSign(val);
	emu.psw.z = getZ(val);
	emu.psw.v = (val == 077777);
	op.a.Store(emu, val);
}
DEF_DISASMS(dec) { InstrOp_mr(opcode).Disasm(os); }
DEF_EXECUTE(decb) {
	PREF_MR_B;
	op.a.Load(emu, &val);
	val = val - 1;
	emu.psw.n = getSign(val);
	emu.psw.z = getZ(val);
	emu.psw.v = (val == 0177);
	op.a.Store(emu, val);
}
DEF_DISASMS(decb) { InstrOp_mr(opcode).Disasm(os); }

DEF_EXECUTE(inc) {
	PREF_MR_W;
	op.a.Load(emu, &val);
	val = val + 1;
	emu.psw.n = getSign(val);
	emu.psw.z = getZ(val);
	emu.psw.v = (val == 0100000);
	op.a.Store(emu, val);
}
DEF_DISASMS(inc) { InstrOp_mr(opcode).Disasm(os); }
DEF_EXECUTE(incb) {
	PREF_MR_B;
	op.a.Load(emu, &val);
	val = val + 1;
	emu.psw.n = getSign(val);
	emu.psw.z = getZ(val);
	emu.psw.v = (val == 0200);
	op.a.Store(emu, val);
}
DEF_DISASMS(incb) { InstrOp_mr(opcode).Disasm(os); }

DEF_EXECUTE(tst) {
	PREF_MR_W;
	op.a.Load(emu, &val);
	emu.psw.n = getSign(val);
	emu.psw.z = getZ(val);
	emu.psw.v = emu.psw.c = 0;
}
DEF_DISASMS(tst) { InstrOp_mr(opcode).Disasm(os); }
DEF_EXECUTE(tstb) {
	PREF_MR_B;
	op.a.Load(emu, &val);
	emu.psw.n = getSign(val);
	emu.psw.z = getZ(val);
	emu.psw.v = emu.psw.c = 0;
}
DEF_DISASMS(tstb) { InstrOp_mr(opcode).Disasm(os); }

DEF_EXECUTE(com) {
	PREF_MR_W;
	op.a.Load(emu, &val);
	val = ~val;
	emu.psw.n = getSign(val);
	emu.psw.z = getZ(val);
	emu.psw.v = 0;
	emu.psw.c = 1;
	op.a.Store(emu, val);
}
DEF_DISASMS(com) { InstrOp_mr(opcode).Disasm(os); }
DEF_EXECUTE(comb) {
	PREF_MR_B;
	op.a.Load(emu, &val);
	val = ~val;
	emu.psw.n = getSign(val);
	emu.psw.z = getZ(val);
	emu.psw.v = 0;
	emu.psw.c = 1;
	op.a.Store(emu, val);
}
DEF_DISASMS(comb) { InstrOp_mr(opcode).Disasm(os); }

DEF_EXECUTE(jmp) {
	PREF_MR_W;
	if (op.a.isReg) {
		emu.RaiseTrap(Emu::TRAP_ILL);
		return;
	}
	emu.genReg[Emu::REG_PC] = op.a.effAddr.ptr;
}
DEF_DISASMS(jmp) { InstrOp_mr(opcode).Disasm(os); }

DEF_EXECUTE(mov) {
	PREF_MRMR_W;
	op.s.Load(emu, &val);
	emu.psw.n = getSign(val);
	emu.psw.z = getZ(val);
	emu.psw.v = 0;
	op.d.Store(emu, val);
}
DEF_DISASMS(mov) { InstrOp_mrmr(opcode).Disasm(os); }
DEF_EXECUTE(movb) {
	PREF_MRMR_B;
	op.s.Load(emu, &val);
	emu.psw.n = getSign(val);
	emu.psw.z = getZ(val);
	emu.psw.v = 0;
	if (op.d.isReg) /* unique movb feature */
		op.d.Store(emu, SignExtend(val));
	else
		op.d.Store(emu, val);
}
DEF_DISASMS(movb) { InstrOp_mrmr(opcode).Disasm(os); }

DEF_EXECUTE(cmp) {
	PREF_MRMR_W;
	op.s.Load(emu, &src);
	op.d.Load(emu, &dst);

	val = src - dst;
	emu.psw.n = getSign(val);
	emu.psw.z = getZ(val);
	emu.psw.v = DetectSubOvf(src, dst, val);
	emu.psw.c = (src < dst);
}
DEF_DISASMS(cmp) { InstrOp_mrmr(opcode).Disasm(os); }
DEF_EXECUTE(cmpb) {
	PREF_MRMR_B;
	op.s.Load(emu, &src);
	op.d.Load(emu, &dst);

	val = src - dst;
	emu.psw.n = getSign(val);
	emu.psw.z = getZ(val);
	emu.psw.v = DetectSubOvf(src, dst, val);
	emu.psw.c = (src < dst);
}
DEF_DISASMS(cmpb) { InstrOp_mrmr(opcode).Disasm(os); }

DEF_EXECUTE(add) {
	PREF_MRMR_W;
	op.s.Load(emu, &src);
	op.d.Load(emu, &dst);
	val = src + dst;
	emu.psw.n = getSign(val);
	emu.psw.z = getZ(val);
	emu.psw.v = DetectAddOvf(src, dst, val);
	emu.psw.c = (val < src);
	op.d.Store(emu, val);
}
DEF_DISASMS(add) { InstrOp_mrmr(opcode).Disasm(os); }

DEF_EXECUTE(sub) {
	PREF_MRMR_W;
	op.s.Load(emu, &src);
	op.d.Load(emu, &dst);
	val = dst - src;
	emu.psw.n = getSign(val);
	emu.psw.z = getZ(val);
	emu.psw.v = DetectSubOvf(src, dst, val);
	emu.psw.c = (dst < src);
	op.d.Store(emu, val);
}
DEF_DISASMS(sub) { InstrOp_mrmr(opcode).Disasm(os); }

DEF_EXECUTE(ash) { /* why it's so complicated :( */
	PREF_RMR; word_t aopv, regv, tmp, res; dword_t ext;
	op.a.Load(emu, &aopv);
	op.r.Load(emu, &regv);

	bool rshift = 040 & aopv;
	aopv &= 037;
	aopv = (rshift) ? (32 - aopv) : aopv;

	bool sign = getSign(regv);
	ext = SignExtend(regv);

	if (!rshift) {
		if (aopv == 0) {
			res = ext;
			emu.psw.v = emu.psw.c = 0;
		} else if (aopv < 16) {
			res = ext << aopv;
			tmp = ext >> (16 - aopv);
			emu.psw.v = (tmp != (getSign(res) ? 0xffff : 0));
			emu.psw.c = tmp & 1;
		} else {
			res = 0;
			emu.psw.v = (ext != 0);
			emu.psw.c = (ext << (aopv - 16)) & 1;
		}
	} else {
		if (aopv == 32) {
			res = -(s_word_t) sign;
			emu.psw.v = 0;
			emu.psw.c = sign;
		} else {
			res = (ext >> aopv) | ((-(s_word_t) sign) << (32 - aopv));
			emu.psw.v = 0;
			emu.psw.c = (ext >> (aopv - 1)) & 1;
		}
	}

	op.r.Store(emu, res);
	emu.psw.n = getSign(res);
	emu.psw.z = getZ(res);
}
DEF_DISASMS(ash) { InstrOp_rmr(opcode).DisasmRSS(os); }

DEF_EXECUTE(mul) {
	PREF_RMR; word_t aopv, regv; s_dword_t val;
	op.a.Load(emu, &aopv);
	op.r.Load(emu, &regv);
	val = SignExtend(aopv) * SignExtend(regv);

	emu.genReg[op.r.effAddr.reg    ] = (val >> 16) & 0xffff;
	emu.genReg[op.r.effAddr.reg | 1] =         val & 0xffff;

	emu.psw.n = getSign((dword_t) val);
	emu.psw.z = getZ((dword_t) val);
	emu.psw.v = 0;
	emu.psw.c = ((val > 077777) || (val < -0100000)); // PDP-11/45 handbook LIES here
}
DEF_DISASMS(mul) { InstrOp_rmr(opcode).DisasmRSS(os); }

DEF_EXECUTE(jsr) {
	PREF_RMR;
	if (op.a.isReg)	{ /* jsr r, r illegal */ 
		emu.RaiseTrap(Emu::TRAP_ILL);
		return;
	}
	word_t tmp, regv;
	auto &sp = emu.genReg[Emu::REG_SP];
	auto &pc = emu.genReg[Emu::REG_PC];
	op.r.Load(emu, &regv);
	tmp = op.a.effAddr.ptr;
       	sp -= sizeof(word_t); emu.Store<word_t>(sp, regv); // sp trap
	op.r.Store(emu, pc);
	pc = tmp;
}
DEF_DISASMS(jsr) { InstrOp_rmr(opcode).DisasmRDD(os); }

DEF_EXECUTE(rts) {
	PREF_R;
	word_t tmp;
	auto &sp = emu.genReg[Emu::REG_SP];
	auto &pc = emu.genReg[Emu::REG_PC];

	op.r.Load(emu, &pc);
	emu.Load<word_t>(sp, &tmp); sp += sizeof(word_t); // sp trap
	op.r.Store(emu, tmp);
}
DEF_DISASMS(rts) { InstrOp_r(opcode).Disasm(os); }


DEF_EXECUTE(halt) { emu.RaiseTrap(Emu::TRAP_ILL); }
DEF_DISASMS(halt) { }


#define DEF_UNIMPL(name)				\
DEF_EXECUTE(name) { emu.RaiseTrap(Emu::TRAP_ILL); }	\
DEF_DISASMS(name) { os << ": unimplemented"; }


DEF_UNIMPL(ashc)
DEF_UNIMPL(div)
DEF_UNIMPL(xor)
DEF_UNIMPL(sob)

DEF_UNIMPL(emt)
DEF_UNIMPL(trap)

DEF_UNIMPL(neg)
DEF_UNIMPL(adc)
DEF_UNIMPL(sbc)
DEF_UNIMPL(ror)
DEF_UNIMPL(rol)
DEF_UNIMPL(asr)
DEF_UNIMPL(asl)

DEF_UNIMPL(negb)
DEF_UNIMPL(adcb)
DEF_UNIMPL(sbcb)
DEF_UNIMPL(rorb)
DEF_UNIMPL(rolb)
DEF_UNIMPL(asrb)
DEF_UNIMPL(aslb)

DEF_UNIMPL(sxt)
DEF_UNIMPL(swab)
DEF_UNIMPL(mtpi)
DEF_UNIMPL(mtpid)
DEF_UNIMPL(mark)
DEF_UNIMPL(mfpi)
DEF_UNIMPL(mfpd)

DEF_UNIMPL(spl)

DEF_UNIMPL(wait)
DEF_UNIMPL(rti)
DEF_UNIMPL(bpt)
DEF_UNIMPL(iot)
DEF_UNIMPL(reset)
DEF_UNIMPL(rtt)

/******************************** FPU ISA *************************************/

DEF_EXECUTE(fpu_unknown) { emu.RaiseTrap(Emu::TRAP_ILL); }
DEF_DISASMS(fpu_unknown) { }

DEF_EXECUTE(setd) { emu.fpu.fpusw.fd = 1; }
DEF_DISASMS(setd) { }

DEF_EXECUTE(seti) { emu.fpu.fpusw.fl = 1; }
DEF_DISASMS(seti) { }

word_t constexpr FPU_ISA_MASK = 0170000;


void Emu::ExecuteInstr(word_t opcode)
{
	if ((opcode & FPU_ISA_MASK) == FPU_ISA_MASK) {
		word_t masked = opcode & ~FPU_ISA_MASK;
#define I_OP(instr) EXECUTE_I(instr, opcode, *this); return;
#include "fpu_isa_switch.h"
#undef I_OP
	} else {
#define I_OP(instr) EXECUTE_I(instr, opcode, *this); return;
#include "isa_switch.h"
#undef I_OP
	}
}

void Emu::DisasmInstr(word_t opcode, std::ostream &os)
{
	if ((opcode & FPU_ISA_MASK) == FPU_ISA_MASK) {
		word_t masked = opcode & ~FPU_ISA_MASK;
#define I_OP(instr) DISASMS_I(instr, opcode, *this, os); return;
#include "fpu_isa_switch.h"
#undef I_OP
	} else {
#define I_OP(instr) DISASMS_I(instr, opcode, *this, os); return;
#include "isa_switch.h"
#undef I_OP
	}
}


#define TRWRAPPER_I(instr) ({					\
	trcache_fn_t fn = []() {				\
		Emu &emu = *Emu::trcache.emu;			\
		/*auto &emupc = emu.genReg[Emu::REG_PC];*/	\
		word_t opcode;					\
		emu.FetchOpcode(opcode);			\
		EXECUTE_I(instr, opcode, emu);			\
		if (emu.trapPending) {				\
			Emu::trcache.trapping_opcode = opcode;	\
			longjmp(Emu::trcache.restore_buf, 1);	\
		}						\
		/* future optimization: update native retaddr if instr changed pc */	\
	};							\
	fn;							\
})

trcache_fn_t Emu::GetTrCacheExecutor(word_t opcode)
{
	if ((opcode & FPU_ISA_MASK) == FPU_ISA_MASK) {
		word_t masked = opcode & ~FPU_ISA_MASK;
#define I_OP(instr) return TRWRAPPER_I(instr);
#include "fpu_isa_switch.h"
#undef I_OP
	} else {
#define I_OP(instr) return TRWRAPPER_I(instr);
#include "isa_switch.h"
#undef I_OP
	}
}
