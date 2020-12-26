.text
.globl _start

.equ VT_XBUF, 0177566

_start:
	mov $0776, sp

	mov $'c, *$VT_XBUF
	mov $'r, *$VT_XBUF
	mov $'t, *$VT_XBUF
	mov $'0, *$VT_XBUF
	mov $'\n, *$VT_XBUF
	halt
	br _start
