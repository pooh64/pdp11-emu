#include <emu.h>
#include <isa.h>
#include <common.h>

#define DEF_EXECUTE(instr) static inline void		\
	Execute_##instr(word_t opcode, struct Emu &emu)

#define DEF_DISASMS(instr) static inline void		\
	Disasms_##instr(word_t opcode, struct Emu &emu, std::ostream &os)

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
		reg += sizeof(T);
		break;
	case 0b011: // *(R)+
		emu.mmu.Load<word_t>(reg, &effAddr.ptr);
		reg += sizeof(word_t);
		break;
	case 0b100: // -(R)
		reg -= sizeof(T);
		effAddr.ptr = reg;
		break;
	case 0b101: // *-(R)
		reg -= sizeof(word_t);
		emu.mmu.Load<word_t>(reg, &effAddr.ptr);
		break;
	case 0b110: // imm(R)
		emu.mmu.Load<word_t>(pc, &imm);
		pc += sizeof(word_t);
		effAddr.ptr = reg + imm;
		break;
	case 0b111: // *imm(R)
		emu.mmu.Load<word_t>(pc, &imm);
		pc += sizeof(word_t);
		emu.mmu.Load<word_t>(reg + imm, &effAddr.ptr);
		break;
	}
}

template <typename T>
inline void AddrOp::Load(Emu &emu, T *val) // may abort
{
	if (isReg)
		*val = emu.genReg[static_cast<Emu::GenRegId>(effAddr.reg)];
	else
		emu.mmu.Load<T>(effAddr.ptr, val);
}

template <typename T>
inline void AddrOp::Store(Emu &emu, T val) // may abort
{
	if (isReg)
		emu.genReg[static_cast<Emu::GenRegId>(effAddr.reg)] = val;
	else
		emu.mmu.Store<T>(effAddr.ptr, val);
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
	AddrOp aop_s, aop_d;
	InstrOp_mrmr(word_t raw) {
		Format fmt; fmt.raw = raw;
		aop_s.init(fmt.m1, fmt.r1);
		aop_d.init(fmt.m2, fmt.r2);
	}
	template<typename T>
	void Fetch(Emu &emu);

	void Disasm(std::ostream &os) {
		os << " ";  aop_s.Disasm(os);
		os << ", "; aop_d.Disasm(os);
	}
};

template<typename T>
void InstrOp_mrmr::Fetch(Emu &emu)
	{ aop_s.Fetch<T>(emu); aop_d.Fetch<T>(emu); } // may abort

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

DEF_EXECUTE(unknown) { emu.RaiseTrap(Emu::TRAP_ILL_INSTR); }
DEF_DISASMS(unknown) { }

DEF_EXECUTE(mov)
{
	InstrOp_mrmr op(opcode); op.Fetch<word_t>(emu);
	word_t val;
	op.aop_s.Load(emu, &val);
	op.aop_d.Store(emu, val);
}
DEF_DISASMS(mov) { InstrOp_mrmr(opcode).Disasm(os); }

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