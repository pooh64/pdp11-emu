static inline unsigned __instr_ash(unsigned v, unsigned s)	/* GCC can't handle rshift */
{
	__asm volatile ("ash %2, %0" : "=r"(v) : "0"(v), "r"(s));
	return v;
}

int strcmp(char const *s1, char const *s2)
{
	while (*s1 && *s2) {
		s1++;
		s2++;
	}
	return *s1 - *s2;
}

#define DL11_RCSR 0177560
#define DL11_RBUF 0177562
#define DL11_XCSR 0177564
#define DL11_XBUF 0177566
#define DL11_CSR_READY 0x80

static inline void vt_putc(char c)
{
	unsigned volatile *xcsr = (typeof(xcsr)) DL11_XCSR;
	char *xbuf = (typeof(xbuf)) DL11_XBUF;

	while (!(*xcsr & DL11_CSR_READY));
	*xbuf = c;
}

int vt_echo = 0;

static inline void vt_getc(char *c)
{
	unsigned volatile *rcsr = (typeof(rcsr)) DL11_RCSR;
	char *rbuf = (typeof(rbuf)) DL11_RBUF;

	while (!(*rcsr & DL11_CSR_READY));
	*c = *rbuf;

	if (vt_echo)
		vt_putc(*c);
}

static void vt_puts(char *str)
{
	while (*str)
		vt_putc(*str++);
}

static char hexdig_strtab[] = {
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	'a', 'b', 'c', 'd', 'e', 'f' };

static void vt_putshex(void *_buf, unsigned sz)
{
	unsigned char *buf = _buf;
	while (sz--) {
		unsigned char c = *buf++;

		/* GCC can't assemble this line properly */
		//unsigned char h = (c >> 4) & 0xf;
		unsigned char h = __instr_ash(c, -4) & 0xf;

		unsigned char l = c & 0xf;
		vt_putc(hexdig_strtab[h]);
		vt_putc(hexdig_strtab[l]);
	}
}

static void vt_getsn(char *str, unsigned n)
{
	char c;
	if (n-- == 0)
		return;

	while (n--) {
		vt_getc(&c);
		if (c == '\n') {
			*str = 0;
			return;
		}
		*str++ = c;
	}
	*str = 0;

	do {
		vt_getc(&c);
	} while (c != '\n');
}

static int fact(int n)
{
	int res = 1;
	while (n > 1)
		res *= (n--);
	return res;
}

void __main()
{
	return;
}

void login()
{
	static char const *LOGIN = "boss";
	static char const *PASSWORD = "secret";

	char buf_login[32], buf_passwd[32];

retry:
	vt_echo = 1;
	vt_puts("login: ");
	vt_getsn(buf_login, sizeof(buf_login));

	vt_echo = 0;
	vt_puts("password: ");
	vt_getsn(buf_passwd, sizeof(buf_passwd));
	vt_putc('\n');

	if (strcmp(buf_login, LOGIN)) {
		vt_puts("login incorrect\n\n");
		goto retry;
	}

	if (strcmp(buf_passwd, PASSWORD)) {
		vt_puts("password incorrect\n\n");
		goto retry;
	}

	vt_puts("\nHello, ");
	vt_puts(buf_login);
	vt_puts("\n\n");
}

typedef unsigned char u8;
typedef unsigned int u16;

void heavycompute()
{
	u8 *beg = (void*) 0x1024;
	u8 *end = (void*) 0x2048;

	u16 accum = 0xdead;

	for (u8 *ptr = beg; ptr < end; ++ptr) {
		accum = __instr_ash(accum, 5) + accum + *ptr;
		accum = __instr_ash(accum, 16 - 7) | __instr_ash(accum, 7);
		accum -= *ptr;
	}

	(void) accum;
}

int main()
{
	for (int i = 0; i < 1024; ++i)
		heavycompute();
	return 0;

	vt_puts("main(): Hello!\n\n");
	login();

	int f = fact(5);
	vt_putshex(&f, sizeof(f));
	vt_puts(" -> thats fact(5) = 0x0078 (le)\n");

	//return 0;

	vt_puts("echo char loop, e to exit\n");
	while (1) {
		char c;
		vt_getc(&c);
		vt_putc(c);
		if (c == 'e')
			break;
	}
	return 0;
}
