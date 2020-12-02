#include <iostream>
#include <fstream>
#include <emu.h>

int main(int argc, char **argv)
{
	if (argc != 2)
		return 1;
	Emu emu;
	std::ifstream test(argv[1], std::ios::binary);
	test.read(reinterpret_cast<char*>(emu.memory.mem), 64);
	test.close();
	emu.genReg[Emu::REG_PC] = 0;

	while (emu.DbgStep(std::cout))
		;
	return 0;
}
