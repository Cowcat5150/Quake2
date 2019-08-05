** From Blitzquake 2014 - Matt Hey asm optimization

**
** Input interrupt routine for mouse control
**
** used for M68k as well as for PowerPC
**
	include "exec/types.i"
	include "devices/inputevent.i"

	code

	xdef    _InputCode
	xdef    InputCode

_InputCode:
InputCode:
	move.l  a0,-(sp)
.loop:
	cmp.b   #IECLASS_RAWMOUSE,(ie_Class,a0)
	bne.b   .next
	move.b  (ie_Code+1,a0),d0
	smi     d1			;if IECODE_UP_PREFIX
	and.w   #$7f,d0			;clear IECODE_UP_PREFIX
	cmp.w   #IECODE_LBUTTON,d0
	beq.b   .lmb
	
.next:
	move.l  (ie_NextEvent,a0),a0
	tst.l   a0
	bne.b   .loop
	move.l  (sp)+,d0
	rts

.lmb:	
	and.w   #IECODE_UP_PREFIX,d1
	move.b  #IECLASS_RAWKEY,ie_Class(a0)
	or.w    #$63,d1
	move.w  d1,(ie_Code,a0)
	bra.b   .next
