#pragma once

struct __putf_str
{
	friend std::ostream &operator<<(std::ostream &os, __putf_str s)
	{ return os << s.str; }
	friend __putf_str putf(char const *fmt...);
private:
	__putf_str(char *buf) { str = buf; }
	char *str;
};

__putf_str putf(char const *fmt...);
