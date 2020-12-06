#include <emu.h>
#include <isa.h>
#include <cstdint>
#include <cinttypes>
#include <common.h>

void Emu::DumpInstr(word_t opcode, std::ostream &os)
{
	os << putf("%.6" PRIo16 " ", opcode);
	Emu::DisasmInstr(opcode, os);
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
