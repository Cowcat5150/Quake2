#ifndef CLIB_WESSUPPORT_H
#define CLIB_WESSUPPORT_H
#ifndef EXEC_LIBRARIES_H
#include <exec/libraries.h>
#endif
#ifndef GRAPHICS_GFX_H
#include <graphics/gfx.h>
#endif
#include <intuition/intuition.h>
#include <libraries/asl.h>
#include <cybergraphx/cybergraphics.h>

#ifdef __PPC
#include <powerpc/warpup_macros.h>
#endif

#define WES_JOYSTICK_PSX_A 0
#define WES_JOYSTICK_PSX_B 1
#define WES_JOYSTICK_PSX_C 2
#define WES_JOYSTICK_PSX_D 3
#define WES_JOYSTICK_PC 4
#define WES_JOYSTICK_SEGA_A 5
#define WES_JOYSTICK_SEGA_B 6
#define WES_JOYSTICK_KEYBOARD 7

#define PIXFMT_AGA -1
#define PIXFMT_AGA_16 -2

struct WES_Offset
{
	int x,y;
};

struct WES_JoyData
{
   signed short x1;
   signed short y1;
   signed short x2;
   signed short y2;
   unsigned char button1;
   unsigned char button2;
   unsigned char button3;
   unsigned char button4;
	 unsigned char buttonl1;
   unsigned char buttonl2;
   unsigned char buttonl3;
   unsigned char buttonr1;
   unsigned char buttonr2;
   unsigned char buttonr3;
   unsigned char buttonup;
   unsigned char buttondown;
   unsigned char buttonleft;
   unsigned char buttonright;
   unsigned char buttonstart;
   unsigned char buttonselect;
	 unsigned int  buttonmask;
};

extern struct Library *WesSupportBase;

#ifndef __INLINE_MACROS_H
/*#ifdef __PPC
#include <powerup/ppcinline/macros.h>
#else*/
#include <inline/macros.h>
*/#endif
#endif /* !__INLINE_MACROS_H */

#ifndef WESSUPPORT_BASE_NAME
#define WESSUPPORT_BASE_NAME WesSupportBase
#endif /* !WESSUPPORT_BASE_NAME */

#define WES_OpenStick(unit,name) LP2(0x48, int, WES_OpenStick, int, unit, d0, char *, name, a0, , WESSUPPORT_BASE_NAME)

#define WES_CloseStick(unit) LP1NR(0x4e, WES_CloseStick, int, unit, d0, , WESSUPPORT_BASE_NAME)

#define WES_ReadStick(unit,data) \
		LP2NR(0x54, WES_ReadStick, int, unit, d0, struct WES_JoyData *, data, a0, \
    , WESSUPPORT_BASE_NAME)
    
#ifdef __PPC
#define WES_OpenStickPPC(v1,v2) PPCLP3(WesSupportBase,-42,int,struct Library *,3,WesSupportBase,int,4,v1,char *,5,v2)
#define WES_CloseStickPPC(v1) PPCLP2NR(WesSupportBase,-48,struct Library *,3,WesSupportBase,int,4,v1)
#define WES_ReadStickPPC(v1,v2) PPCLP3NR(WesSupportBase,-54,struct Library *,3,WesSupportBase,int,4,v1,struct WES_JoyData *,5,v2)
#define WES_ChunkyCopyPPC(v1,v2,v3,v4,v5,v6) PPCLP7NR(WesSupportBase,-30,struct Library *,3,WesSupportBase,void *,4,v1,void *,5,v2,int,6,v3,int,7,v4,int,8,v5,int,9,v6)
#define WES_ChunkyCopyFullPPC(v1,v2,v3,v4,v5,v6,v7) PPCLP8NR(WesSupportBase,-36,struct Library *,3,WesSupportBase,void *,4,v1,void *,5,v2,struct WES_Offset *,6,v3,struct WES_Offset *,7,v4,struct WES_Offset *,8,v5,int,9,v6,int,10,v7)
#define WES_OpenSoundPPC(v1,v2,v3,v4) PPCLP5(WesSupportBase,-60,int,struct Library *,3,WesSupportBase,int,4,v1,char *,5,v2,void *,6,v3,int,7,v4)
#define WES_CloseSoundPPC() LP1NR(WesSupportBase,-66,struct Library *,3,WesSupportBase)
#endif

#endif
