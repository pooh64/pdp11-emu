.globl _start
.globl __main
.globl _main

.equ VT_XBUF, 0177566

.text

_start:
	mov $01000, sp

	jsr pc, pretty_display
	jsr pc, _main
	halt

pretty_display:
	mov $'c, *$VT_XBUF
	mov $'r, *$VT_XBUF
	mov $'t, *$VT_XBUF
	mov $'0, *$VT_XBUF
	mov $'\n, *$VT_XBUF
	rts pc
