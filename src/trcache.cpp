#include "emu.h"
#include "isa.h"
#include <cstdint>
#include <cinttypes>
#include "common.h"

#define frame_retaddr (*((void**) __builtin_frame_address(0) + 1))

Emu::TrCache Emu::trcache;

static inline size_t PtrToTrCache(word_t ptr)
{
	return ptr / sizeof(word_t);
}

static void TrCacheHook() {
	auto &emu = *Emu::trcache.emu;
	auto &pc = emu.genReg[Emu::REG_PC];
	word_t op;
	Emu::trcache.emu-> Load(pc, &op);
	size_t pos = PtrToTrCache(pc);

	Emu::trcache.cache[pos].exec = Emu::GetTrCacheExecutor(op);
	Emu::trcache.cache[pos].exec();
}

static void FillHooks(Emu::TrCache &trcache) {
	for (size_t i = 0; i < Emu::TrCache::sz; ++i)
		trcache.cache[i].exec = &TrCacheHook;
}

void Emu::TrCacheAcquire()
{
	Emu::trcache.emu = this;
}

Emu::TrCache::TrCache() {
	cache = new trcache_entry[Emu::TrCache::sz];
	FillHooks(*this);
}

Emu::TrCache::~TrCache() {
	delete[] cache;
}

void Emu::TrCacheRun(std::ostream &os)
{
	Emu &emu = *Emu::trcache.emu;
	auto &emupc = emu.genReg[Emu::REG_PC];
	if (setjmp(Emu::trcache.restore_buf))
		goto restored;
	while (1) {
		Emu::trcache.cache[PtrToTrCache(emupc)].exec();
	}
	restored:
	if (emu.trapPending)
		goto trapped;
	assert(0 && "restored with no reason");
	return;
trapped:
	os << "\tTrap raised: ";
	emu.DumpTrap(emu.trapId, os);
	os << "\n\t";
	emu.DumpInstr(emu.trcache.trapping_opcode, os);
	os << "\n";
	emu.DumpReg(os);
	os << "\n";
	return;
}

void Emu::TrCacheStep(std::ostream &os)
{
	Emu &emu = *Emu::trcache.emu;
#ifdef CONF_DUMP_REG
	emu.DumpReg(os);
	os << "\n";
#endif
	auto &emupc = emu.genReg[Emu::REG_PC];
#ifdef CONF_DUMP_INSTR
	word_t opcode_dump;
	emu.Load(emu.genReg[REG_PC], &opcode_dump);
	emu.DumpInstr(opcode_dump, os);
	os << "\n";
#endif
	if (setjmp(Emu::trcache.restore_buf))
		goto restored;
	Emu::trcache.cache[PtrToTrCache(emupc)].exec();
	return;

restored:
	if (emu.trapPending)
		goto trapped;
	assert(0 && "restored with no reason");
	return;
trapped:
	os << "\tTrap raised: ";
	emu.DumpTrap(emu.trapId, os);
	os << "\n\t";
	emu.DumpInstr(emu.trcache.trapping_opcode, os);
	os << "\n";
	emu.DumpReg(os);
	os << "\n";
	return;
}