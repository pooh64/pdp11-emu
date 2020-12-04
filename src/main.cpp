#include <iostream>
#include <fstream>
#include <emu.h>

int main(int argc, char **argv)
{
	word_t const load_addr = 01000;

	if (argc != 2)
		return 1;
	Emu emu;
	std::ifstream test(argv[1], std::ios::binary);
	test.read(reinterpret_cast<char*>(emu.coreMem.mem + load_addr), 64);
	test.close();
	emu.genReg[Emu::REG_PC] = load_addr;

	while (emu.DbgStep(std::cout))
		;
	return 0;
}
