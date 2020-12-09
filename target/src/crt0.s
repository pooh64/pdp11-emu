.text
.globl _start

.equ VT_XBUF, 0177566

_start:
	mov $0776, sp

	mov $'h, *$VT_XBUF
	mov $'e, *$VT_XBUF
	mov $'l, *$VT_XBUF
	mov $'l, *$VT_XBUF
	mov $'o, *$VT_XBUF
	mov $010,*$VT_XBUF
	mov $' , *$VT_XBUF
	halt
	br _start
