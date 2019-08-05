*
* Sound interrupt code
*

* intdata_t offsets
m_pBufferLeft   EQU     0
m_pBufferRight  EQU     4
m_pTimerBase    EQU     8
m_nIntMask      EQU     12
m_tv            EQU     16

* other custom chip registers
intreq          EQU     $09C

* timer.device functions
GetSysTime      EQU     -66

*
* Interrtupt Code
* Relevant registers:
* A0    custom chip base
* A1    intdata pointer
* A6    SysBase

	section code,code

_intcode:
    	movem.l a0/a1/d0/d1/a6,-(sp)    * save registers

    	move.l  m_pTimerBase(a1),a6     * Get timer base
    	move.l  a1,a0                   * Get the timval address
    	add.l   #m_tv,a0
    	jsr     GetSysTime(a6)          * Get time

    	movem.l (sp)+,a0/a1/d0/d1/a6    * Restore registers

    	move.l  m_nIntMask(a1),d0       * Clear the interrupt
    	move.w  d0,intreq(a0)

    	moveq   #0,d0                   * Flag the interrupt as handled

    	rts

    	end



