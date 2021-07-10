#pragma once
#include "emu.h"

#define EXECUTE_I(instr, op, emu) Execute_##instr(op, emu)
#define DISASMS_I(instr, op, emu, os)  do { os << #instr; Disasms_##instr(opcode, emu, os); } while (0)
