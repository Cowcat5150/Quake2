// in_amiga.c
#include "../client/client.h"
#include "in.h"

cvar_t *in_joystick;

int psxused;

cvar_t *joy_pc;
cvar_t *joy_psx = 0;

int only_pc=0;

// PC Analog Joystick disabled by now. Cowcat
//void IN_ShutdownPC();
//void IN_JoyMovePC();
//void IN_InitPC();
void IN_ShutdownPsx();
void IN_JoyMovePsx();
void IN_InitPsx();
void IN_InitJoystick();
void IN_ShutdownJoystick();

extern void IN_InitJoy();
extern void IN_JoyMove();
extern void IN_StartupJoystick();

void IN_Init (void)
{
	psxused = 0;
	only_pc = 0;

	joy_pc = Cvar_Get("joy_pc", "0", CVAR_ARCHIVE);
	joy_psx = Cvar_Get("joy_psx", "0", CVAR_ARCHIVE);

	Com_Printf("Initializing Mouse\n");
	IN_InitMouse();

	if (joy_pc->value)
	{
		only_pc = 1;
		Com_Printf("Initializing Analog Joystick\n");
		//IN_InitPC();
		return;
	}

	if (joy_psx->value)
	{
		Com_Printf("Initializing PSX pad\n");
		psxused = 1;
		IN_InitPsx();
	}

	IN_InitJoy();
	IN_StartupMouse();
	IN_StartupJoystick();
}

void IN_Shutdown (void)
{
	IN_ShutdownMouse();

	if (only_pc)
	{
		//IN_ShutdownPC();
			return;
	}

	if (psxused)
		IN_ShutdownPsx();

	IN_ShutdownJoystick();
}

void IN_Frame (void)
{
}

void IN_Move (usercmd_t *cmd)
{
	IN_MouseMove(cmd);

	if (only_pc)
	{
		//IN_JoyMovePC(cmd);
			return;
	}

	if (psxused)
		IN_JoyMovePsx(cmd);

	IN_JoyMove(cmd);
}

void IN_Activate (qboolean active)
{
}


