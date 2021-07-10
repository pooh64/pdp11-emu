#include "emu.h"
#include "isa.h"
#include <cstdint>
#include <cinttypes>
#include "common.h"

#include <sys/mman.h>

#include "trcache.h"

void *xexec_alloc(size_t sz) {
	void *ptr = mmap(NULL, sz + sizeof(size_t), PROT_READ | PROT_WRITE | PROT_EXEC,
		MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
	if (ptr == MAP_FAILED)
		abort();
	size_t *mem = (size_t*) ptr;
	*(mem++) = (size_t) sz;
	return mem;
}

void xexec_free(void *ptr) {
	size_t *mem = (size_t*) ptr;
	size_t sz = *(--mem);
	if (munmap(mem, sz + sizeof(size_t)) < 0)
		abort();
}

Emu::TrCache Emu::trcache;

#ifdef CONF_ENABLE_TRCACHE
static void TrCacheHook() {
	auto &emu = *Emu::trcache.emu;
	auto &pc = emu.genReg[Emu::REG_PC];
	word_t op;
	Emu::trcache.emu-> Load(pc, &op);
	size_t pos = PtrToTrCache(pc);
	//std::cout << "hook: " << pos << "\n";

	Emu::trcache.cache[pos].set(Emu::GetTrCacheExecutor(op));
#ifdef CONF_ENABLE_TRCACHE_RUN_INLINE
	frame_retaddr_shift(-sizeof(trcache_entry));
#else
	Emu::trcache.cache[pos].exec();
#endif
}
#else
static void TrCacheHook() {
	// dummy
}
#endif

static void FillHooks(Emu::TrCache &trcache) {
	for (size_t i = 0; i < Emu::TrCache::sz; ++i)
		trcache.cache[i].set(&TrCacheHook);
}

void Emu::TrCacheAcquire()
{
	Emu::trcache.emu = this;
}

Emu::TrCache::TrCache() {
	cache = (trcache_entry*) xexec_alloc(sizeof(trcache_entry) * Emu::TrCache::sz);
	FillHooks(*this);
}

Emu::TrCache::~TrCache() {
	xexec_free(cache);
}

#ifdef CONF_ENABLE_TRCACHE
void Emu::TrCacheRun(std::ostream &os)
{
	Emu &emu = *Emu::trcache.emu;
	auto &emupc = emu.genReg[Emu::REG_PC];
	auto cache = Emu::trcache.cache;
	if (setjmp(Emu::trcache.restore_buf))
		goto restored;
#ifdef CONF_ENABLE_TRCACHE_RUN_INLINE
	void *callptr;
	callptr = (void*) &cache[PtrToTrCache(emupc)];
	((void(*)()) callptr)();
#else
	while (1) {
		Emu::trcache.cache[PtrToTrCache(emupc)].exec();
	}
#endif

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
#else
void Emu::TrCacheRun(std::ostream &os) {
	// dummy
}
#endif


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