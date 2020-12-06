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

static inline bool getZ(byte_t val) { return val; }
static inline bool getZ(word_t val) { return val; }

/* Addressing unit (src/dst) */
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
		break;
	case 0b011: // *(R)+
		emu.Load<word_t>(reg, &effAddr.ptr);
		reg += sizeof(word_t);
		break;
	case 0b100: // -(R)
		reg -= (op_reg < Emu::REG_SP) ? sizeof(T) : sizeof(word_t);
		effAddr.ptr = reg;
		break;
	case 0b101: // *-(R)
		reg -= sizeof(word_t);
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
			unsigned r2 : 3;
			unsigned m2 : 3;
			unsigned r1 : 3;
			unsigned m1 : 3;
			unsigned op : 4;
		};
		word_t raw;
	};
	AddrOp as, ad;
	InstrOp_mrmr(word_t raw) {
		Format fmt; fmt.raw = raw;
		as.init(fmt.m1, fmt.r1);
		ad.init(fmt.m2, fmt.r2);
	}
	template<typename T>
	void Fetch(Emu &emu);

	void Disasm(std::ostream &os) {
		os << " ";  as.Disasm(os);
		os << ", "; ad.Disasm(os);
	}
};

template<typename T>
void InstrOp_mrmr::Fetch(Emu &emu)
	{ as.Fetch<T>(emu); ad.Fetch<T>(emu); } // may abort

struct InstrOp_rmr {
	unsigned br : 3;
	unsigned bm : 3;
	unsigned ar : 3;
	unsigned op : 7;
};

struct InstrOp_mr {
	unsigned ar : 3;
	unsigned am : 3;
	unsigned op : 10;
};

struct InstrOp_r {
	unsigned r : 3;
	unsigned op : 13;
};

DEF_EXECUTE(unknown) { emu.RaiseTrap(Emu::TRAP_ILL); }
DEF_DISASMS(unknown) { }

#define PREF_MRMR_W InstrOp_mrmr op(opcode); op.Fetch<word_t>(emu); word_t val;
#define PREF_MRMR_B InstrOp_mrmr op(opcode); op.Fetch<byte_t>(emu); byte_t val;

DEF_EXECUTE(mov) {
	PREF_MRMR_W;
	op.as.Load(emu, &val);
	emu.psw.n = getSign(val);
	emu.psw.z = getZ(val);
	emu.psw.v = 0;
	op.ad.Store(emu, val);
}
DEF_DISASMS(mov) { InstrOp_mrmr(opcode).Disasm(os); }

DEF_EXECUTE(movb) {
	PREF_MRMR_B;
	op.as.Load(emu, &val);
	emu.psw.n = getSign(val);
	emu.psw.z = getZ(val);
	emu.psw.v = 0;
	if (op.ad.isReg) /* unique movb feature */
		op.ad.Store(emu, SignExtend(val));
	else
		op.ad.Store(emu, val);
}
DEF_DISASMS(movb) { InstrOp_mrmr(opcode).Disasm(os); }

void Emu::ExecuteInstr(word_t opcode)
{
#define I_OP(instr) EXECUTE_I(instr, opcode, *this); break;
#include <isa_switch.h>
#undef I_OP
}

void Emu::DisasmInstr(word_t opcode, std::ostream &os)
{
#define I_OP(instr) DISASMS_I(instr, opcode, *this, os); break;
#include <isa_switch.h>
#undef I_OP
}
