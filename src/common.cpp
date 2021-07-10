#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <new>

#include <common.h>

thread_local static char *__putf_buf = NULL;

__attribute__((destructor))
static void __destroy_putf_buf() { free(__putf_buf); }

__putf_str putf(char const *fmt...)
{
	va_list args;
	va_start(args, fmt);
	free(__putf_buf); /* I want to reuse it, but i can't :( */
	if (vasprintf(&__putf_buf, fmt, args) < 0) {
		std::bad_alloc exc;
		throw exc;
	}
	va_end(args);

	return __putf_str(__putf_buf);
}
