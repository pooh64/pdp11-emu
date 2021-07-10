#pragma once
#include <iostream>
#include <cassert>
#include <vector>
#include <csetjmp>

#include "configure.h"

using    byte_t = uint8_t;
using    word_t = uint16_t;
using  s_word_t =  int16_t;
using   dword_t = uint32_t;
using s_dword_t =  int32_t;

using trcache_fn_t = void (*)();
struct trcache_entry {
	trcache_fn_t exec;
	trcache_entry() {};
};
struct Emu {
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
		MAX_REG	= REG_PC + 1,
	};
	enum PSWMode : uint8_t {
		PSW_KERNEL = 0,
		PSW_SUPERV = 1,
		PSW_USERMD = 2,
		PSW_MODE_MAX = 3,
	};

	/* Processor status word */
	struct PSW {
		union __attribute__((may_alias)) {
			struct __attribute__((packed)) {
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
			};
			word_t raw = 0;
		};
	};

	struct FPU {
		union __attribute__((may_alias)) FPUSW {
			struct __attribute__((packed)) {
				unsigned fc  : 1;
				unsigned fv  : 1;
				unsigned fz  : 1;
				unsigned fn  : 1;
				unsigned fmm : 1;
				unsigned ft  : 1;
				unsigned fl  : 1;
				unsigned fd  : 1;
				unsigned ic  : 1;
				unsigned iv  : 1;
				unsigned iu  : 1;
				unsigned iuv : 1;
				unsigned _unused : 2;
				unsigned fid : 1;
				unsigned fer : 1;
			};
			word_t raw = 0;
		};
		FPUSW fpusw;
	};

	/* General registers */
	struct GenRegFile {
		word_t reg[MAX_REG] = { };
		word_t spSet[PSW_MODE_MAX] = { };
		word_t savedSet[REG_SET] = { };
		uint8_t setId = 0;
		uint8_t spMode = 0;
		void ChangeSet(uint8_t newId);
		void ChangeSP(PSWMode newMode);
		word_t &operator[](uint8_t id) { return reg[id]; /* unaligned sp/pc? */ }
	};

	enum TrapId : uint8_t {
#define DEF_TRAP(name, vec, str) TRAP_##name,
#include "trap_def.h"
#undef DEF_TRAP
		MAX_TRAP
	};

	enum TrapVec : uint8_t {
#define DEF_TRAP(name, vec, str) TRAP_VEC_##name = vec,
#include "trap_def.h"
#undef DEF_TRAP
	};

	void RaiseTrap(TrapId const trap) { trapId = trap; trapPending = true; };

	static constexpr word_t IO_PAGE_BASE = (64 - 4) * 1024;

	struct CoreMemory {
		friend struct MMU;
		static constexpr word_t sz = IO_PAGE_BASE;
		bool PAExist(word_t ptr) { return ptr < sz; }
		byte_t *mem;
		CoreMemory() { mem = new byte_t[sz]; }
		~CoreMemory() { delete[] mem; }
	};
	static struct TrCache {
		static constexpr size_t sz = (IO_PAGE_BASE / sizeof(word_t));
		Emu *emu;
		trcache_entry *cache;
		jmp_buf restore_buf;
		word_t trapping_opcode;
		TrCache();
		~TrCache();
	} trcache; // static!!!
	void TrCacheAcquire();

	struct DevBase {
		virtual void Load(Emu &emu, word_t ptr, byte_t *buf, uint8_t sz)=0;
		virtual void Store(Emu &emu, word_t ptr, byte_t *buf, uint8_t sz)=0;
	};

	struct DevInfo {
		word_t ptr;
		word_t len;
		DevBase *dev;
	};


	FPU fpu;
	GenRegFile genReg;
	CoreMemory coreMem;
	PSW psw;
	TrapId trapId;
	TrapVec trapVec;
	bool trapPending = false; /* =? PSW.val.t */
	std::vector<DevInfo> devices;

	void IOspaceRegister(DevInfo &dev) { devices.push_back(dev); }

	template<typename T>
	static bool IsPtrAligned(word_t ptr) {return !(ptr % sizeof(T));}

	template<typename T> void Load(word_t ptr, T *val);
	template<typename T> void Store(word_t ptr, T val);

	void AdvancePC() { genReg[REG_PC] += sizeof(word_t); }
	void FetchOpcode(word_t &opcode) { Load(genReg[REG_PC], &opcode); AdvancePC(); }
	void ExecuteInstr(word_t opcode);

	void DisasmInstr(word_t opcode, std::ostream &os);
	void DumpInstr(word_t opcode, std::ostream &os);
	void DumpTrap(Emu::TrapId t, std::ostream &os);
	void DumpReg(std::ostream &os);

	void DbgStep(std::ostream &os);

	static trcache_fn_t GetTrCacheExecutor(word_t opcode);

	static void InitTrCachePc(word_t opcode);
	static void TrCacheStep(std::ostream &os);
	static void TrCacheRun(std::ostream &os);

	Emu() { }

private:
	bool IOspaceFind(word_t ptr, DevInfo &dev);
	template<typename T> void IOspaceLoad(word_t ptr, T *val);
	template<typename T> void IOspaceStore(word_t ptr, T val);
	template<typename T> bool CheckAlign(word_t ptr);
};

inline bool Emu::IOspaceFind(word_t ptr, DevInfo &dev)
{
	for (auto &d : devices) {
		word_t tmp = ptr - d.ptr;
		if (tmp >= 0 && tmp < d.len) {
			dev = d;
			return true;
		}
	}
	return false;
}
template<typename T>
inline void Emu::IOspaceLoad(word_t ptr, T *val)
{
	DevInfo dev;
	if (!IOspaceFind(ptr, dev)) {
		RaiseTrap(TRAP_MME); return;
	}
	ptr -= dev.ptr;
	dev.dev->Load(*this, ptr, reinterpret_cast<byte_t*>(val), sizeof(*val));
}
template<typename T>
inline void Emu::IOspaceStore(word_t ptr, T val)
{
	DevInfo dev;
	if (!IOspaceFind(ptr, dev)) {
		RaiseTrap(TRAP_MME); return;
	}
	ptr -= dev.ptr;
	dev.dev->Store(*this, ptr, reinterpret_cast<byte_t*>(&val), sizeof(val));
}
template<typename T>
inline bool Emu::CheckAlign(word_t ptr)
{
	if (!IsPtrAligned<T>(ptr)) {
		RaiseTrap(TrapId::TRAP_ODD);
		return false;
	}
	return true;
}
template<typename T>
inline void Emu::Load(word_t ptr, T *val)
{
	if (!CheckAlign<T>(ptr)) return;
	if (ptr >= IO_PAGE_BASE) {
		IOspaceLoad(ptr, val); return;
	}
	if (ptr >= coreMem.sz) {
		RaiseTrap(TRAP_MME); return;
	}
	*val = *(reinterpret_cast<T*>(&coreMem.mem[ptr]));
}
template<typename T>
inline void Emu::Store(word_t ptr, T val)
{
	if (!CheckAlign<T>(ptr)) return;
	if (ptr >= IO_PAGE_BASE) {
		IOspaceStore(ptr, val); return;
	}
	if (ptr >= coreMem.sz) {
		RaiseTrap(TRAP_MME); return;
	}
	*(reinterpret_cast<T*>(&coreMem.mem[ptr])) = val;
}
