#include "../client/client.h"
#include "in.h"

#ifdef __VBCC__
#pragma amiga-align
#elif defined(WARPUP)
#pragma pack(push,2)
#endif

#include <intuition/intuition.h>
#include <proto/intuition.h>
#include <exec/interrupts.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <intuition/intuitionbase.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>

#ifdef __PPC__
#if defined(__GNUC__)
#include <powerpc/powerpc.h>
#include <powerpc/powerpc_protos.h>
#else
#include <powerpc/powerpc.h>
#include <proto/powerpc.h>
#endif
#endif

#ifdef __VBCC__
#pragma default-align
#elif defined(WARPUP)
#pragma pack(pop)
#endif

cvar_t	    *m_filter;
cvar_t	    *in_mouse;

qboolean    mlooking;

int	    mouse_x, mouse_y;
int	    old_mouse_x, old_mouse_y;

qboolean    mouseactive;
qboolean    mouseinitialized;

// Cowcat windowmode MouseHandler stuff 

static struct MsgPort	*InputPort=NULL;
static struct IOStdReq	*InputIO=NULL;
static struct Interrupt InputHandler;

extern cvar_t *vid_fullscreen;

/*68k code */
extern void *InputCode(void);

BOOL mhandler=FALSE;

//unsigned short *HandlerPointer = 0;

extern struct Window *win;
////

void IN_MLookDown (void)
{
	mlooking = true;
}

void IN_MLookUp (void)
{
	mlooking = false;

	if (!freelook->value && lookspring->value)
		IN_CenterView ();
}

/*
===========
IN_ActivateMouse

Called when the window gains focus or changes in some way
===========
*/

void IN_ActivateMouse (void)
{
}

/*
===========
IN_DeactivateMouse

Called when the window loses focus
===========
*/
void IN_DeactivateMouse (void)
{
}

void MouseHandlerOff (void)
{	 
	if (mhandler)
	{
		InputIO->io_Data=(void *)&InputHandler;
		InputIO->io_Command=IND_REMHANDLER;
		DoIO((struct IORequest *)InputIO);

		CloseDevice((struct IORequest *)InputIO);		
		DeleteIORequest((struct IORequest *)InputIO);
		DeleteMsgPort(InputPort);
	
		mhandler = FALSE; 
		//ModifyIDCMP(win, IDCMP_RAWKEY|IDCMP_MOUSEBUTTONS);
		win->IDCMPFlags |= IDCMP_RAWKEY|IDCMP_MOUSEBUTTONS;
	}
}

void MouseHandler (void)
{
	if (!vid_fullscreen->value && !mhandler)
	{
		if ( InputPort = CreateMsgPort() )
		{
			if (InputIO = (struct IOStdReq *) CreateIORequest(InputPort, sizeof(struct IOStdReq)))
			{
				OpenDevice("input.device",0,(struct IORequest *)InputIO, 0);

				InputHandler.is_Node.ln_Type = NT_INTERRUPT;
				InputHandler.is_Node.ln_Pri = 90;
				InputHandler.is_Code = (void(*)())InputCode;

				InputIO->io_Data = (void *)&InputHandler;
				InputIO->io_Command = IND_ADDHANDLER;

				DoIO((struct IORequest *)InputIO);
	  
				mhandler = TRUE;
				//ModifyIDCMP(win, IDCMP_RAWKEY|IDCMP_MOUSEMOVE|IDCMP_MOUSEBUTTONS|IDCMP_DELTAMOVE);
				win->IDCMPFlags |= IDCMP_RAWKEY|IDCMP_MOUSEMOVE|IDCMP_MOUSEBUTTONS|IDCMP_DELTAMOVE;
			}
		}
	}
}

/*
===========
IN_StartupMouse
===========
*/

void IN_StartupMouse (void)
{	
	cvar_t *cv;

	mouseinitialized = FALSE;
	mouseactive = FALSE;

	// if ( COM_CheckParm ("-nomouse") )
	//	return;

	cv = Cvar_Get ("in_initmouse", "1", CVAR_NOSET);

	if ( cv && !cv->value )
		return;

	Com_Printf("Mouse initialized\n");
	
	mouseinitialized = TRUE;
	mouseactive = TRUE;
}

/*
===========
IN_MouseEvent
===========
*/

void IN_MouseEvent (int mstate)
{
}

static int MouseX;
static int MouseY;

void IN_GetMouseMove(struct IntuiMessage *msg)
{
	MouseX = msg->MouseX;
	MouseY = msg->MouseY;
}


/*
===========
IN_MouseMove
===========
*/

void IN_MouseMove (usercmd_t *cmd)
{
	int mx, my;

	if (!mouseactive)
		return;

	mx = (float)MouseX;
	my = (float)MouseY;

	MouseX = 0;
	MouseY = 0;
       
#if 0
	if (!mx && !my)
		return;
#endif

	if (m_filter->value)
	{
		mouse_x = (mx + old_mouse_x) * 0.5;
		mouse_y = (my + old_mouse_y) * 0.5;
	}

	else
	{
		mouse_x = mx;
		mouse_y = my;
	}

	old_mouse_x = mx;
	old_mouse_y = my;

	mouse_x *= (sensitivity->value*10);
	mouse_y *= (sensitivity->value*10);

	// add mouse X/Y movement to cmd

	if ( (in_strafe.state & 1) || (lookstrafe->value && mlooking ))
		cmd->sidemove += m_side->value * mouse_x;

	else
		cl.viewangles[YAW] -= m_yaw->value * mouse_x;

	if ( (mlooking || freelook->value) && !(in_strafe.state & 1))
	{
		cl.viewangles[PITCH] += m_pitch->value * mouse_y;
	}

	else
	{
		cmd->forwardmove -= m_forward->value * mouse_y;
	}

}

void IN_InitMouse()
{
	// mouse variables
	m_filter = Cvar_Get ("m_filter", "0", 0);
	in_mouse = Cvar_Get ("in_mouse", "1", CVAR_ARCHIVE);

	// Mouse commands
	Cmd_AddCommand ("+mlook", IN_MLookDown);
	Cmd_AddCommand ("-mlook", IN_MLookUp);

	// Start the mouse
	if (mouseinitialized == false)
		IN_StartupMouse();
}


/*
===================
IN_ClearStates
===================
*/

void IN_ClearStates (void)
{
}

void IN_ShutdownMouse(void)
{
	//Cowcat 
	if (mhandler) 
		MouseHandlerOff();
	//

}
