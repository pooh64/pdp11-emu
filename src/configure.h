#pragma once

//#define CONF_DUMP_INSTR
//#define CONF_DUMP_REG
//#define CONF_SHOW_CYCLES

/* use switch-translation caching */
#define CONF_ENABLE_TRCACHE

/* use translation cache in loop */
/* 50% faster than vanilla */
/* 35M cycles/sec coreI5 6gen */
#define CONF_ENABLE_TRCACHE_RUN

/* emit amd64 code in translation cache - JIT */
/* 20% slower than vanilla 😅 */
#define CONF_ENABLE_TRCACHE_RUN_INLINE
