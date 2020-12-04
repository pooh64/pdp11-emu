#pragma once
#include <emu.h>

#define DEF_EXECUTE(instr) static inline void		\
	Execute_##instr(word_t op, struct Emu &emu)

#define DEF_DISASMS(instr) static inline void		\
	Disasms_##instr(word_t op, std::ostream &os)

DEF_EXECUTE(unknown) { emu.RaiseTrap(Emu::TRAP_ILL_INSTR); }
DEF_DISASMS(unknown) { }


template<typename T>
static inline T LoadOperand(Emu &emu, uint8_t op)
{
	Emu::GenRegId op_reg = (op & 0b111);
	word_t &reg = emu.genReg[op_reg];
	word_t &pc = emu.genReg[Emu::REG_PC];

	uint8_t op_mode = (op >> 3 & 0b111);

	T val; word_t ptr; word_t imm;
	switch (op_mode) {
		case 0b000: // R
			val = static_cast<T>(reg);
			break;
		case 0b001: // @R
			emu.mmu.Load<T>(reg, &val);
			break;
		case 0b010: // (R)+
			emu.mmu.Load<T>(reg, &val);
			reg += sizeof(T);
			break;
		case 0b011: // @(R)+
			emu.mmu.Load<word_t>(reg, &ptr);
			emu.mmu.Load<T>(ptr, &val);
			reg += sizeof(word_t);
			break;
		case 0b100: // -(R)
			reg -= sizeof(T);
			emu.mmu.Load<T>(reg, &val);
			break;
		case 0b101: // @-(R)
			reg -= sizeof(word_t);
			emu.mmu.Load<word_t>(reg, &ptr);
			emu.mmu.Load<T>(ptr, &val);
			break;
		case 0b110: // imm(R)
			emu.mmu.Load<word_t>(pc + sizeof(word_t), &imm);
			emu.mmu.Load<T>(reg + imm, &val);
			break;
		case 0b111: // @imm(R)
			emu.mmu.Load<word_t>(pc + sizeof(word_t), &imm);
			emu.mmu.Load<word_t>(reg + imm, &ptr);
			emu.mmu.Load<T>(ptr, &val);
			break;
	}
	// restore ?
}

#define EXECUTE_I(instr, op, emu) Execute_##instr(op, emu)
#define DISASMS_I(instr, op, os)  do { os << #instr; Disasms_##instr(op, os); } while (0)
