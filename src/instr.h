#pragma once
#include <emu.h>

#define DEF_EXECUTE(instr) static inline void		\
	Execute_##instr(word_t op, struct Emu &emu)

#define DEF_DISASMS(instr) static inline void		\
	Disasms_##instr(word_t op, struct Emu &emu, std::ostream &os)

#define EXECUTE_I(instr, op, emu) Execute_##instr(op, emu)
#define DISASMS_I(instr, op, emu, os)  do { os << #instr; Disasms_##instr(op, emu, os); } while (0)

template<typename T>
struct __OperandLoader {
	void OpReg(Emu &emu, word_t *reg, T *val)
	{
		*val = static_cast<T>(*reg);
	}
	void OpMem(Emu &emu, word_t ptr, T *val)
	{
		emu.mmu.Load<T>(ptr, val);
	}
};

template<typename T>
struct __OperandStorer {
	void OpReg(Emu &emu, word_t *reg, T *val)
	{
		*reg = static_cast<word_t>(*val);
	}
	void OpMem(Emu &emu, word_t ptr, T *val)
	{
		emu.mmu.Store<T>(ptr, *val);
	}
};

// sign ext?
template<typename T, typename F>
static inline void __AccessOperand(Emu &emu, uint8_t op, T &val)
{
	F f;
	Emu::GenRegId op_reg = static_cast<Emu::GenRegId>(op & 0b111);
	word_t &reg = emu.genReg[op_reg];
	word_t &pc = emu.genReg[Emu::REG_PC];

	uint8_t op_mode = ((op >> 3) & 0b111);

	word_t ptr; word_t imm;
	switch (op_mode) {
		case 0b000: // R
			f.OpReg(emu, &reg, &val);
			break;
		case 0b001: // (R)
			f.OpMem(emu, reg, &val);
			break;
		case 0b010: // (R)+
			f.OpMem(emu, reg, &val);
			reg += sizeof(T);
			break;
		case 0b011: // *(R)+
			emu.mmu.Load<word_t>(reg, &ptr);
			f.OpMem(emu, ptr, &val);
			reg += sizeof(word_t);
			break;
		case 0b100: // -(R)
			reg -= sizeof(T);
			f.OpMem(emu, reg, &val);
			break;
		case 0b101: // *-(R)
			reg -= sizeof(word_t);
			emu.mmu.Load<word_t>(reg, &ptr);
			f.OpMem(emu, ptr, &val);
			break;
		case 0b110: // imm(R)
			emu.mmu.Load<word_t>(pc, &imm);
			pc = pc + sizeof(word_t);
			f.OpMem(emu, reg + imm, &val);
			break;
		case 0b111: // *imm(R)
			emu.mmu.Load<word_t>(pc, &imm);
			pc = pc + sizeof(word_t);
			emu.mmu.Load<word_t>(reg + imm, &ptr);
			f.OpMem(emu, ptr, &val);
			break;
	}
	// trap -> restore+raise ?
}

template <typename T>
static inline void OPLoad(Emu &emu, uint8_t op, T *val)
{
	__AccessOperand<T, __OperandLoader<T>>(emu, op, *val);
}

template <typename T>
static inline void OPStore(Emu &emu, uint8_t op, T val)
{
	__AccessOperand<T, __OperandStorer<T>>(emu, op, val);
}

static void DumpOperand(Emu &emu, uint8_t op, std::ostream &os);

DEF_EXECUTE(unknown) { emu.RaiseTrap(Emu::TRAP_ILL_INSTR); }
DEF_DISASMS(unknown) { }

DEF_EXECUTE(mov) { word_t val; OPLoad(emu, op >> 6, &val); OPStore(emu, op, val); /* flags */ }
DEF_DISASMS(mov) { os << " "; DumpOperand(emu, op >> 6, os); os << ", "; DumpOperand(emu, op, os); }

DEF_EXECUTE(movb) { /* not impl */}
DEF_DISASMS(movb) { os << " "; DumpOperand(emu, op >> 6, os); os << ", "; DumpOperand(emu, op, os); }
