
	.text
	.align	2
	.global RmTailPPC
	.type 	RmTailPPC, @function

# Hedeon rip

RmTailPPC:
	lwz	%r5,8(%r4)
	lwz	%r3,4(%r5)
	mr.	%r3,%r3
	beq-	.TaillistEmpty
	stw	%r3,8(%r4)
	addi	%r4,%r4,4
	stw	%r4,0(%r3)
	mr	%r3,%r5
.TaillistEmpty: blr
	
	.size	RmTailPPC, .-RmTailPPC

