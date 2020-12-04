#include <emu.h>
#include <instr.h>
#include <cstdint>
#include <cinttypes>
#include <common.h>

void Emu::ExecuteInstr(word_t opcode)
{
#define I_OP(instr) EXECUTE_I(instr, opcode, *this); break;
#include <instr_switch.h>
#undef I_OP
}

void Emu::DisasmInstr(word_t opcode, std::ostream &os)
{
#define I_OP(instr) DISASMS_I(instr, opcode, *this, os); break;
#include <instr_switch.h>
#undef I_OP
}

void Emu::DumpInstr(word_t opcode, std::ostream &os)
{
	os << putf("%.6" PRIo16 " ", opcode);
	Emu::DisasmInstr(opcode, os);
}

static void DumpOperand(Emu &emu, uint8_t op, std::ostream &os)
{
	Emu::GenRegId op_reg = static_cast<Emu::GenRegId>(op & 0b111);
	uint8_t op_mode = ((op >> 3) & 0b111);

	switch (op_mode) {
		case 0b000: // R
			os << putf("r%d", op_reg);
			break;
		case 0b001: // (R)
			os << putf("(r%d)", op_reg);
			break;
		case 0b010: // (R)+
			os << putf("(r%d)+", op_reg);
			break;
		case 0b011: // *(R)+
			os << putf("*(r%d)+", op_reg);
			break;
		case 0b100: // -(R)
			os << putf("-(r%d)", op_reg);
			break;
		case 0b101: // *-(R)
			os << putf("*-(r%d)", op_reg);
			break;
		case 0b110: // imm(R)
			os << putf("imm(r%d)", op_reg);
			break;
		case 0b111: // *imm(R)
			os << putf("*imm(r%d)", op_reg);
			break;
	}
}

void DumpTrap(Emu::TrapVec t, std::ostream &os)
{
	os << putf("%.3" PRIo8, t);
}

void DumpReg(Emu::GenRegFile &r, std::ostream &os)
{
	for (uint8_t i = Emu::REG_R0; i < Emu::REG_MAX; ++i)
		os << putf("r%u=%.6" PRIo16 " ", i, r[static_cast<Emu::GenRegId>(i)]);
}

void Emu::DbgStep(std::ostream &os)
{
	word_t opcode;
	DumpReg(genReg, os);
	os << "\n\t";
	FetchOpcode(opcode);
	if (trapPending)
		goto trapped;
	DumpInstr(opcode, os);
	os << "\n";
	ExecuteInstr(opcode);
	if (trapPending) goto trapped;
	return;
trapped:
	os << "\ttrap raised: ";
	DumpTrap(trapVec, os);
	os << "\n";
	return;
}
