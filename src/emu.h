#pragma once
#include <iostream>
#include <assert.h>

using byte_t = uint8_t;
using word_t = uint16_t;
using dword_t = uint32_t;

struct Emu {
	template<typename T>
	static bool IsPtrAligned(word_t ptr) {return !(ptr % sizeof(T));}

	enum GenRegId : uint8_t {
		REG_R0	= 00,
		REG_R1	= 01,
		REG_R2	= 02,
		REG_R3	= 03,
		REG_R4	= 04,
		REG_R5	= 05,
		REG_SP	= 06,
		REG_PC	= 07,
		REG_SET = REG_R5 + 1,
		REG_MAX	= REG_PC + 1,
	};
	enum PSWMode {
		PSW_KERNEL = 0,
		PSW_SUPERV = 1,
		PSW_USERMD = 2,
		PSW_MODE_MAX = 3,
	};

	/* Processor status word */
	struct PSW {
		__attribute__((packed)) struct PSWValue {
			unsigned c : 1;
			unsigned v : 1;
			unsigned z : 1;
			unsigned n : 1;
			unsigned t : 1;
			unsigned prio		: 3;
			unsigned reserved	: 3;
			unsigned regSet		: 1;
			unsigned prevMode	: 2;
			unsigned curMode	: 2;
		} val;
	};

	struct GenRegFile {
		word_t reg[REG_MAX] = { };
		word_t spSet[PSW_MODE_MAX] = { };
		word_t savedSet[REG_SET] = { };
		uint8_t setId = 0;
		uint8_t spMode = 0;
		void pswSet(uint8_t newId)
		{
			assert(newId == 0 || newId == 1);
			if (setId != newId) {
				setId = newId;
				for (uint8_t i = REG_R0; i < REG_SET; ++i)
					std::swap(savedSet[i], reg[i]);
			}
		}
		void pswSP(PSWMode newMode)
		{
			assert(newMode < PSW_MODE_MAX);
			spSet[spMode] = reg[REG_SP];
			reg[REG_SP] = spSet[spMode = newMode];
		}
		word_t &operator[](GenRegId id) { return reg[id]; }
	} genReg;

	/* Trap Vector */
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

	struct CoreMemory {
		friend struct MMU;
		const dword_t sz = (128 - 4) * 1024;
		byte_t *mem;
		CoreMemory() { mem = new byte_t[sz]; }
		~CoreMemory() { delete[] mem; }
	} coreMem;

	struct MMU {
		Emu &emu;
		CoreMemory &coreMem;
		MMU(Emu &_e, CoreMemory &_cm) : emu(_e), coreMem(_cm) { }
	private:
		template<typename T> bool CheckAlign(word_t ptr)
		{
			if (!IsPtrAligned<T>(ptr)) {
				emu.RaiseTrap(TrapVec::TRAP_ODD_ADDR);
				return false;
			}
			return true;
		}
	public:
		template<typename T> void Load(word_t ptr, T *val)
		{
			if (!CheckAlign<T>(ptr)) return;
			*val = *(reinterpret_cast<T*>(&coreMem.mem[ptr]));
		}
		template<typename T> void Store(word_t ptr, T val)
		{
			if (!CheckAlign<T>(ptr)) return;
			*(reinterpret_cast<T*>(&coreMem.mem[ptr])) = val;
		}
	} mmu;

	void FetchOpcode(word_t &opcode) { mmu.Load(genReg[REG_PC], &opcode); }
	void AdvancePC() { genReg[REG_PC] += sizeof(word_t); }

	void ExecuteInstr(word_t opcode);
	void DisasmInstr(word_t opcode, std::ostream &os);
	void DumpInstr(word_t opcode, std::ostream &os);
	bool DbgStep(std::ostream &os);

	Emu() : mmu(*this, coreMem) { }
};
