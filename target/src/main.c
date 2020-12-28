void __main()
{
	return;
}

#define DL11_RCSR 0177560
#define DL11_RBUF 0177562
#define DL11_XCSR 0177564
#define DL11_XBUF 0177566
#define DL11_CSR_READY 0x80

static void vt_putc(char c)
{
	unsigned volatile *xcsr = (typeof(xcsr)) DL11_XCSR;
	unsigned char *xbuf = (typeof(xbuf)) DL11_XBUF;

	while (!(*xcsr & DL11_CSR_READY));
	*xbuf = c;
}

static void vt_getc(unsigned char *c)
{
	unsigned volatile *rcsr = (typeof(rcsr)) DL11_RCSR;
	unsigned char *rbuf = (typeof(rbuf)) DL11_RBUF;

	while (!(*rcsr & DL11_CSR_READY));
	*c = *rbuf;
}

int main()
{
	while (1) {
		unsigned char c;
		vt_getc(&c);
		vt_putc(c);
	}
}
