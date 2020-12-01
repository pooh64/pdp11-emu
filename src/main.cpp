#include <iostream>
#include <emu.h>

int main()
{
	Emu::Instr instr;
	word_t opcode = 0x15c0;
	DecodeInstr(opcode, instr);
	std::cout << "Opcode: " << opcode << " : " << instr << std::endl;
	return 0;
}
