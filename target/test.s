	.text
	.even
	.globl	_start
_start:
	mov	$02000, r1
	mov	$0010531, patch_me
patch_me:
	halt
	mov	$0004, *(r0)+
	mov	$0012, -(r1)
	mov	(r1)+, r2
	mov	r1, r2
	mov	r2, r3
	mov	$000, *-(r2)
	mov 	r7, 22(r3)
	mov	$010, *22(r3)
	halt
