#include <emu.h>
#include <isa.h>
#include <cstdint>
#include <cinttypes>
#include <common.h>

void Emu::DumpReg(std::ostream &os)
{
	for (uint8_t i = Emu::REG_R0; i < Emu::MAX_REG; ++i)
		os << putf("r%u=%.6" PRIo16 " ", i, genReg[static_cast<Emu::GenRegId>(i)]);
}

void Emu::DbgStep(std::ostream &os)
{
	word_t opcode;
#ifdef CONF_DUMP_REG
	DumpReg(os);
	os << "\n";
#endif
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
	DumpTrap(trapId, os);
	os << "\n\t";
	DumpReg(os);
	os << "\n";
	return;
}

void Emu::GenRegFile::ChangeSet(uint8_t newId)
{
	assert(newId == 0 || newId == 1);
	if (setId != newId) {
		setId = newId;
		for (uint8_t i = REG_R0; i < REG_SET; ++i)
			std::swap(savedSet[i], reg[i]);
	}
}

void Emu::GenRegFile::ChangeSP(PSWMode newMode)
{
	assert(newMode < PSW_MODE_MAX);
	spSet[spMode] = reg[REG_SP];
	reg[REG_SP] = spSet[spMode = newMode];
}

void Emu::DumpInstr(word_t opcode, std::ostream &os)
{
	os << putf("%.6" PRIo16 " ", opcode);
	Emu::DisasmInstr(opcode, os);
}

void Emu::DumpTrap(Emu::TrapId t, std::ostream &os)
{
	static const char *strTab[] = {
#define DEF_TRAP(name, vec, str) [Emu::TRAP_##name] = str,
#include <trap_def.h>
#undef DEF_TRAP
	};
	os << strTab[t];
}
