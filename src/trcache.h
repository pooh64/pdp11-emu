#pragma once

#include <stdint.h>
#include "emu.h"

#define log_trcache() do {					\
	auto emupc = Emu::trcache.emu->genReg[Emu::REG_PC];	\
	void *retaddr = frame_retaddr;				\
	std::cout << emupc << " " << (((size_t) retaddr) / sizeof(trcache_entry)) - 1 << std::endl;	\
} while (0)

#define frame_retaddr (*((void**) __builtin_frame_address(0) + 1))

#define frame_retaddr_shift(offs) do {			\
	size_t *rap = (size_t*) &frame_retaddr;		\
	*rap += (offs);					\
} while(0)

struct __attribute__((packed)) trcache_entry {
	uint64_t _mov_rax : 16;
	uint64_t _mov_imm : 64;
	uint64_t _call_rax : 16;

	inline void set(trcache_fn_t fn) {
		_mov_rax = 0xb848;
		_mov_imm = (uint64_t) fn;
		_call_rax = 0xd0ff;
	}

	inline void exec() {
		((trcache_fn_t) _mov_imm)();
	}
};

static inline size_t PtrToTrCache(word_t ptr)
{
	return ptr / sizeof(word_t);
}

