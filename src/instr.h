#pragma once
#include <emu.h>

#define DEF_EXECUTE(instr) static inline void		\
	Execute_##instr(word_t op, struct Emu &emu)

#define DEF_DISASMS(instr) static inline void		\
	Disasms_##instr(word_t op, std::ostream &os)

DEF_EXECUTE(unknown) { emu.RaiseTrap(Emu::TRAP_ILL_INSTR); }
DEF_DISASMS(unknown) {}

#define EXECUTE_I(instr, op, emu) Execute_##instr(op, emu)
#define DISASMS_I(instr, op, os)  do { os << #instr; Disasms_##instr(op, os); } while (0)
