#include <iostream>

using byte_t = uint8_t;
using word_t = uint16_t;
template<typename T> bool IsPtrAligned(word_t ptr);
template<> bool IsPtrAligned<byte_t>(word_t ptr) { return true; }
//template<> bool IsPtrAlgined<word_t>(word_t ptr) { return !(ptr % sizeof(word_t)); }

struct Emu {
	enum struct TrapVec : byte_t {
		USER_DEV	= 0000,
		ODD_ADDR	= 0004,
		ILL_INSTR	= 0004,
		STK_RED		= 0004,
		RES_INSTR	= 0010,
		TRACE		= 0014,
		POWER_FAIL	= 0024,
		EMT		= 0030,
		TRAP		= 0034,
		MM_TRAP		= 0250,
	} trapVec;
	bool trapPending = false;
	void RaiseTrap(TrapVec const trap) { trapVec = trap; trapPending = true; };

	enum struct GenRegId : byte_t {
		R0	= 00,
		R1	= 01,
		R2	= 02,
		R3	= 03,
		R4	= 04,
		R5	= 05,
		R6	= 06,
		R7	= 07,
		PC	= R7,
		HW_SP	= R6,
		MAX	= 07,
	};
	word_t genRegfile[static_cast<byte_t>(GenRegId::MAX)];

	struct Memory {
		Emu &emu;
		byte_t *dummy;
		word_t const dummy_start = 00377;
		word_t const dummy_end   = 01377;
		Memory(Emu &_emu) : emu(_emu) { dummy = new byte_t[dummy_end - dummy_start + 1]; }
		~Memory() { delete[] dummy; }
		bool IsMapped(word_t ptr) { return ptr >= dummy_start && ptr <= dummy_end; }
	private:
		template<typename T> bool CheckPtr(word_t ptr) {
			if (!IsMapped(ptr)) {
				emu.RaiseTrap(TrapVec::MM_TRAP);
				return false;
			}
			if (!IsPtrAligned<T>(ptr)) {
				emu.RaiseTrap(TrapVec::ODD_ADDR);
				return false;
			}
			return true;
		}
	public:
		template<typename T> bool Load(word_t ptr, T &val) {
			if (!CheckPtr<T>(ptr))
				return false;
			val = *(reinterpret_cast<T*>(&dummy[ptr-dummy_start]));
			return true;
		}
	} memory;

	struct InstrBase {
		virtual void Execute(struct Emu &emu)=0;
		virtual std::ostream &operator<<(std::ostream &os)=0;
	};

	struct UnknownInstr : public InstrBase {
		UnknownInstr() {}
		void Execute(struct Emu &emu) { emu.RaiseTrap(TrapVec::ILL_INSTR); }
		std::ostream &operator<<(std::ostream &os) { return os << "Unknown instr"; }
	};

	union __attribute__((may_alias)) Instr {
		Instr() { }
		void Execute(struct Emu &emu) { getBase()->Execute(emu); }
		friend std::ostream &operator<<(std::ostream &os, Instr &i) { return i.getBase()->operator<<(os); }
	private:
		InstrBase *getBase() { return reinterpret_cast<InstrBase*>(this); }
		UnknownInstr unknown;
	};

	word_t ReadPC() { return genRegfile[static_cast<byte_t>(GenRegId::PC)]; }
	void AdvancePC() { genRegfile[static_cast<byte_t>(GenRegId::PC)] += sizeof(word_t); }

	bool FetchOpcode(word_t &opcode) { return memory.Load(ReadPC(), opcode); }

	void ExecuteInstr()
	{
		/* F D, check trap */
		/* call Execute, check trap */
		AdvancePC();
	}

	Emu() : memory(*this) { }
};

void DecodeInstr(word_t opcode, Emu::Instr &instr)
{
	uint8_t lvl = opcode >> (16-3);
	switch (lvl) {
		default:
			new(&instr) Emu::UnknownInstr();
	}
}
