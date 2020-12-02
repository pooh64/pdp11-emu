#include <emu.h>
#include <instr.h>

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

bool Emu::DbgStep(std::ostream &os)
{
	word_t opcode;
	FetchOpcode(opcode);
	std::ios_base::fmtflags fl(os.flags());
	if (trapPending) {
		os << "fetch: ";
		goto trapped;
	}
	os << "0x" << opcode << " ";
	os.flags(fl);
	DisasmInstr(opcode, os);
	ExecuteInstr(opcode);
	if (trapPending) goto trapped;
	os << "\n";
	AdvancePC();
	return true;
trapped:
	os << "\ntrap raised: " << trapVec << "\n";
	return false;
}
