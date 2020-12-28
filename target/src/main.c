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

static inline void vt_getc(char *c)
{
	unsigned volatile *rcsr = (typeof(rcsr)) DL11_RCSR;
	char *rbuf = (typeof(rbuf)) DL11_RBUF;

	while (!(*rcsr & DL11_CSR_READY));
	*c = *rbuf;
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
		vt_putc(hexdig_strtab[*buf >> 4]);
		vt_putc(hexdig_strtab[*buf]);
		buf++;
	}
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

int main()
{
	vt_puts("main(): HELLO!\n");
	int f = fact(5);
	vt_putshex(&f, sizeof(f));
	vt_putc('\n');

	while (1) {
		char c;
		vt_getc(&c);
		vt_putc(c);
	}
}
