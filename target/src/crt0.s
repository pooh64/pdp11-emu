.globl _start
.globl __main
.globl _main

.text

_start:
	mov $01000, sp

	jsr pc, _main
	halt
