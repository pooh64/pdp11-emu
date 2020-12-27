#include <emu.h>
#include <isa.h>
#include <common.h>

#define DEF_EXECUTE(instr) static inline void		\
	Execute_##instr(word_t opcode, struct Emu &emu)

#define DEF_DISASMS(instr) static inline void		\
	Disasms_##instr(word_t opcode, struct Emu &emu, std::ostream &os)

static inline bool getSign(byte_t val) { return 0x80 & val; }
static inline bool getSign(word_t val) { return 0x8000 & val; }

static inline word_t SignExtend(byte_t val)
{
	return (0x80 & val) ? 0xff00 | val : val;
}

static inline bool getZ(byte_t val) { return !val; }
static inline bool getZ(word_t val) { return !val; }

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

template <typename T>
void AddrOp::Fetch(Emu &emu)	// Execute unit to get effAddr, may abort
{
	word_t &pc = emu.genReg[Emu::REG_PC];
	word_t &reg = emu.genReg[static_cast<Emu::GenRegId>(op_reg)];
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
		*val = emu.genReg[static_cast<Emu::GenRegId>(effAddr.reg)];
	else
		emu.Load<T>(effAddr.ptr, val);
}

template <typename T>
inline void AddrOp::Store(Emu &emu, T val) // may abort
{
	if (isReg)
		emu.genReg[static_cast<Emu::GenRegId>(effAddr.reg)] = val;
	else
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
#define PREF_MRMR_W InstrOp_mrmr op(opcode); op.Fetch<word_t>(emu); word_t val;
#define PREF_MRMR_B InstrOp_mrmr op(opcode); op.Fetch<byte_t>(emu); byte_t val;
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
#define PREF_MR_W InstrOp_mr op(opcode); op.Fetch<word_t>(emu); word_t val;
#define PREF_MR_B InstrOp_mr op(opcode); op.Fetch<byte_t>(emu); byte_t val;
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


static inline
void ExecuteBranch(struct Emu &emu, word_t raw)
{
	s_word_t offs = SignExtend(raw & 0xff);
	emu.genReg[Emu::REG_PC] += 2 * offs;
}

#define DEF_BRANCH_LIST		\
	DEF_BRANCH(br, true)

#define DEF_BRANCH(name, pred)	\
DEF_EXECUTE(name) { if (pred) ExecuteBranch(emu, opcode); }
DEF_BRANCH_LIST
#undef DEF_BRANCH

#define DEF_BRANCH(name, pred)	DEF_DISASMS(name) { }
DEF_BRANCH_LIST
#undef DEF_BRANCH

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
		os << " se";
	else
		os << " cl";

	for (int i = 0; i < 4; ++i, op.raw >>= 1) {
		if (op.raw & 1)
			os << " " << CCODEopStrtab[i];
	}
}

DEF_EXECUTE(unknown) { emu.RaiseTrap(Emu::TRAP_ILL); }
DEF_DISASMS(unknown) { }

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

DEF_EXECUTE(add) {
	PREF_MRMR_W;
	word_t tmp1, tmp2;
	op.s.Load(emu, &tmp2);
	op.d.Load(emu, &tmp1);
	val = tmp1 + tmp2;
	emu.psw.n = getSign(val);
	emu.psw.z = getZ(val);
	emu.psw.v = !(getSign(tmp1) ^ getSign(tmp2)) && (getSign(tmp1) ^ getSign(val));
	emu.psw.c = getSign(tmp1) ^ getSign(val);
	op.d.Store(emu, val);
}
DEF_DISASMS(add) { InstrOp_mrmr(opcode).Disasm(os); }

DEF_EXECUTE(jsr) {
	PREF_RMR;
	if (op.a.isReg)	/* jsr r, r illegal */
		emu.RaiseTrap(Emu::TRAP_ILL);
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
#include <fpu_isa_switch.h>
#undef I_OP
	} else {
#define I_OP(instr) EXECUTE_I(instr, opcode, *this); return;
#include <isa_switch.h>
#undef I_OP
	}
}

void Emu::DisasmInstr(word_t opcode, std::ostream &os)
{
	if ((opcode & FPU_ISA_MASK) == FPU_ISA_MASK) {
		word_t masked = opcode & ~FPU_ISA_MASK;
#define I_OP(instr) DISASMS_I(instr, opcode, *this, os); return;
#include <fpu_isa_switch.h>
#undef I_OP
	} else {
#define I_OP(instr) DISASMS_I(instr, opcode, *this, os); return;
#include <isa_switch.h>
#undef I_OP
	}
}
