#include <emu.h>
#include <instr.h>
#include <cstdint>
#include <cinttypes>
#include <common.h>

void Emu::ExecuteInstr(word_t opcode)
{
#define I_OP(instr) EXECUTE_I(instr, opcode, *this)
#include <instr_switch.h>
#undef I_OP
}

void Emu::DisasmInstr(word_t opcode, std::ostream &os)
{
#define I_OP(instr) DISASMS_I(instr, opcode, os)
#include <instr_switch.h>
#undef I_OP
}

void Emu::DumpInstr(word_t opcode, std::ostream &os)
{
	/*
	std::ios_base::fmtflags fl(os.flags());
	os << "\t0" << std::oct << genReg[REG_PC];
	os << "\t0" << std::oct << opcode << "\t";
	os.flags(fl);
	*/
	os << putf("%.7" PRIo16 " %.7" PRIo16 " ", genReg[REG_PC], opcode);
	Emu::DisasmInstr(opcode, os);
}

bool Emu::DbgStep(std::ostream &os)
{
	word_t opcode;
	FetchOpcode(opcode);
	if (trapPending)
		goto trapped;
	DumpInstr(opcode, os);
	os << "\n";
	ExecuteInstr(opcode);
	if (trapPending) goto trapped;
	AdvancePC();
	return true;
trapped:
	os << "\ttrap raised: " << trapVec << "\n";
	return false;
}
