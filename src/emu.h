#pragma once
#include <iostream>

using byte_t = uint8_t;
using word_t = uint16_t;

struct Emu {
	template<typename T>
	static bool IsPtrAligned(word_t ptr) {return !(ptr % sizeof(T));}

	enum TrapVec {
		TRAP_USER_DEV	= 0000,
		TRAP_ODD_ADDR	= 0004,
		TRAP_ILL_INSTR	= 0004,
		TRAP_STK_RED	= 0004,
		TRAP_RES_INSTR	= 0010,
		TRAP_TRACE	= 0014,
		TRAP_POWER_FAIL	= 0024,
		TRAP_EMT	= 0030,
		TRAP_TRAP	= 0034,
		TRAP_MM_TRAP	= 0250,
	} trapVec;
	bool trapPending = false;
	void RaiseTrap(TrapVec const trap) { trapVec = trap; trapPending = true; };

	enum GenRegId {
		REG_R0	= 00,
		REG_R1	= 01,
		REG_R2	= 02,
		REG_R3	= 03,
		REG_R4	= 04,
		REG_R5	= 05,
		REG_R6	= 06,
		REG_R7	= 07,
		REG_PC	= REG_R7,
		REG_SP	= REG_R6,
		REG_MAX	= 07,
	};
	word_t genReg[REG_MAX];

	struct Memory {
		Emu &emu;
		byte_t *mem;

		Memory(Emu &_emu) : emu(_emu) { mem = new byte_t[64*1024]; }
		~Memory() { delete[] mem; }
	private:
		template<typename T> void CheckPtr(word_t ptr)
		{
			if (!IsPtrAligned<T>(ptr))
				emu.RaiseTrap(TrapVec::TRAP_ODD_ADDR);
		}
	public:
		template<typename T> void Load(word_t ptr, T &val)
		{
			CheckPtr<T>(ptr);
			if (emu.trapPending)
				return;
			val = *(reinterpret_cast<T*>(&mem[ptr]));
		}
		template<typename T> void Store(word_t ptr, T &val)
		{
			CheckPtr<T>(ptr);
			if (emu.trapPending)
				return;
			*(reinterpret_cast<T*>(&mem[ptr])) = val;
		}
	} memory;

	void FetchOpcode(word_t &opcode) { memory.Load(genReg[REG_PC], opcode); }
	void AdvancePC() { genReg[REG_PC] += sizeof(word_t); }

	void ExecuteInstr(word_t opcode);
	void DisasmInstr(word_t opcode, std::ostream &os);
	bool DbgStep(std::ostream &os);

	Emu() : memory(*this) { }
};
