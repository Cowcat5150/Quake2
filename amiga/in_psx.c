#include "../client/client.h"
#include "in.h"

#ifdef __VBCC__
#pragma amiga-align
#elif defined(WARPUP)
#pragma pack(push,2)
#endif
#include <clib/exec_protos.h>
#include <proto/exec.h>
#include <devices/gameport.h>

#ifdef __VBCC__
#pragma default-align
#elif defined(WARPUP)
#pragma pack(pop)
#endif

#include "psxport.h"

extern cvar_t  *joy_advanced;
extern cvar_t  *joy_forwardthreshold;
extern cvar_t  *joy_sidethreshold;
extern cvar_t  *joy_upthreshold;
extern cvar_t  *joy_pitchthreshold;
extern cvar_t  *joy_yawthreshold;
extern cvar_t  *joy_pitchsensitivity;
extern cvar_t  *joy_yawsensitivity;

extern cvar_t  *joy_side_reverse;
extern cvar_t  *joy_up_reverse;
extern cvar_t  *joy_forward_reverse;
cvar_t *joy_onlypsx=0L;
cvar_t *psx_sensitivity;
cvar_t *psx_deadzone;
cvar_t *psx_showaxis;
cvar_t *psx_joyspeed;
cvar_t *psx_deadzone2;


struct JoyStickDesc
{
    int axis;
    int mv0;
    int mv1;
    int value,notvalue;
};

static struct JoyStickDesc tj;

int analog_centered=FALSE;
int analog_clx, analog_cly;
int analog_lx, analog_ly;
int analog_crx, analog_cry;
int analog_rx, analog_ry;

struct GamePortTrigger gameport_gpt = {
  GPTF_UPKEYS | GPTF_DOWNKEYS,  /* gpt_Keys */
  0,                /* gpt_Timeout */
  1,                /* gpt_XDelta */
  1             /* gpt_YDelta */
};

static struct InputEvent gameport_ie;
 
int psxport=0; 
extern int psxused;

static struct MsgPort *gameport_mp = NULL;
static struct IOStdReq *gameport_io = NULL;
static BYTE gameport_ct;
static int gameport_is_open = FALSE;
static int gameport_io_in_progress = FALSE;
static int gameport_curr,gameport_prev;

void ResetPsx(void)
{ 
  int ix;


      if ((gameport_mp = CreateMsgPort ()) == NULL ||
	  (gameport_io = (struct IOStdReq *)CreateIORequest (gameport_mp, sizeof(struct IOStdReq))) == NULL)
	Sys_Error ("Can't create MsgPort or IoRequest");

      for (ix=0; ix<4; ix++)
      {
	if (OpenDevice ("psxport.device", ix, (struct IORequest *)gameport_io, 0) == 0)
	{
	  Com_Printf ("\npsxport.device unit %d opened.\n",ix);
	  gameport_io->io_Command = GPD_ASKCTYPE;
	  gameport_io->io_Length = 1;
	  gameport_io->io_Data = &gameport_ct;
	  DoIO ((struct IORequest *)gameport_io);
	  if (gameport_ct == GPCT_NOCONTROLLER)
	  {
	    gameport_is_open = TRUE;
	    ix=4;
	  }
	  else
	  {
	    CloseDevice ((struct IORequest *)gameport_io);
	  }
	}
      }


  if (!gameport_is_open) 
  {
    Com_Printf ("No psxport, or no free psx controllers!  Joystick disabled.\n");
    psxused = 0;
  } 
  else 
  {

    gameport_ct = GPCT_ALLOCATED;
    gameport_io->io_Command = GPD_SETCTYPE;
    gameport_io->io_Length = 1;
    gameport_io->io_Data = &gameport_ct;
    DoIO ((struct IORequest *)gameport_io);

    gameport_io->io_Command = GPD_SETTRIGGER;
    gameport_io->io_Length = sizeof(struct GamePortTrigger);
    gameport_io->io_Data = &gameport_gpt;
    DoIO ((struct IORequest *)gameport_io);

    gameport_io->io_Command = GPD_READEVENT;
    gameport_io->io_Length = sizeof (struct InputEvent);
    gameport_io->io_Data = &gameport_ie;
    SendIO ((struct IORequest *)gameport_io);
    gameport_io_in_progress = TRUE;
  }
} 

void IN_ShutdownPsx(void) 
{ 
 if (!gameport_is_open)
    return;
 if (gameport_io_in_progress) 
 {
   AbortIO ((struct IORequest *)gameport_io);
   WaitIO ((struct IORequest *)gameport_io);
   gameport_io_in_progress = FALSE;
   gameport_ct = GPCT_NOCONTROLLER;
   gameport_io->io_Command = GPD_SETCTYPE;
   gameport_io->io_Length = 1;
   gameport_io->io_Data = &gameport_ct;
   DoIO ((struct IORequest *)gameport_io);
 }
 if (gameport_is_open) 
 {
   CloseDevice ((struct IORequest *)gameport_io);
   gameport_is_open = FALSE;
 }
 if (gameport_io != NULL) 
 {
   DeleteIORequest ((struct IORequest *)gameport_io);
   gameport_io = NULL;
 }
 if (gameport_mp != NULL) 
 {
   DeleteMsgPort (gameport_mp);
   gameport_mp = NULL;
 }
} 

void GetValuesPsx(usercmd_t *cmd)
{
  int x,y; 

  if (GetMsg (gameport_mp) != NULL) 
  {
    if ((gameport_ie.ie_Class == PSX_CLASS_JOYPAD) 
     || (gameport_ie.ie_Class == PSX_CLASS_WHEEL)) 
      analog_centered = false;

    if (PSX_CLASS(gameport_ie) != PSX_CLASS_MOUSE)
    {
      gameport_curr = ~PSX_BUTTONS(gameport_ie);
      
      if (gameport_curr !=gameport_prev)
      {

	if (gameport_curr & PSX_TRIANGLE) tj.value = tj.value|1;
	else if (tj.value&1)
	{
	  tj.value=tj.value&~1;
	  tj.notvalue|=1;
	}

	if (gameport_curr & PSX_SQUARE) tj.value=tj.value|4;
	else if (tj.value&4)
	{
	  tj.value=tj.value&~4;
	  tj.notvalue|=4;
	}

	if (gameport_curr & PSX_CIRCLE) tj.value=tj.value|8;
	else if (tj.value&8)
	{
	  tj.value=tj.value&~8;
	  tj.notvalue|=8;
	}

	if (gameport_curr & PSX_CROSS) tj.value=tj.value|2;
	else if (tj.value&2)
	{
	  tj.value=tj.value&~2;
	  tj.notvalue|=2;
	}
  
	if (gameport_curr & PSX_LEFT) tj.mv1=0;
	else if (gameport_curr & PSX_RIGHT) tj.mv1=65536;
	else tj.mv1=32768;
       
	if (gameport_curr & PSX_UP) tj.mv0=0;
	else if (gameport_curr & PSX_DOWN) tj.mv0=65536;
	else tj.mv0=32768;

	if (gameport_curr & PSX_START) tj.value=tj.value|64;
	else if (tj.value&64)
	{
	  tj.value=tj.value&~64;
	  tj.notvalue|=64;
	}

	if (gameport_curr & PSX_SELECT) tj.value=tj.value|128;
	else if (tj.value&128)
	{
	  tj.value=tj.value&~128;
	  tj.notvalue|=128;
	}

	if (gameport_curr & PSX_R1) tj.value=tj.value|16;
	else if (tj.value&16)
	{
	  tj.value=tj.value&~16;
	  tj.notvalue|=16;
	}

	if (gameport_curr & PSX_L1) tj.value=tj.value|32;
	else if (tj.value&32)
	{
	  tj.value=tj.value&~32;
	  tj.notvalue|=32;
	}

	if (gameport_curr & PSX_R2) tj.value=tj.value|256;
	else if (tj.value&256)
	{
	  tj.value=tj.value&~256;
	  tj.notvalue|=256;
	}

	if (gameport_curr & PSX_L2) tj.value=tj.value|512;
	else if (tj.value&512)
	{
	  tj.value=tj.value&~512;
	  tj.notvalue|=512;
	}

	gameport_prev = gameport_curr;

      }

    }

    if ((PSX_CLASS(gameport_ie) == PSX_CLASS_ANALOG) 
     || (PSX_CLASS(gameport_ie) == PSX_CLASS_ANALOG2) 
     || (PSX_CLASS(gameport_ie) == PSX_CLASS_ANALOG_MODE2)) 
    {
      analog_lx = PSX_LEFTX(gameport_ie);
      analog_ly = PSX_LEFTY(gameport_ie);
      analog_rx = PSX_RIGHTX(gameport_ie);
      analog_ry = PSX_RIGHTY(gameport_ie);

      if (!analog_centered) 
      {
	analog_clx = analog_lx;
	analog_cly = analog_ly;
	analog_crx = analog_rx;
	analog_cry = analog_ry;
	analog_centered = true;
      }          

      x=analog_lx-analog_clx;
      y=analog_cly-analog_ly;
      if (psx_showaxis->value) Com_Printf("Left: (%d %d)  ", x, y);
      if ((abs(x)>psx_deadzone->value) || (abs(y)>psx_deadzone->value))
      {
	x<<=1;
	y<<=1;
	x=x*psx_sensitivity->value;
	y=y*psx_sensitivity->value;
    if ( (in_strafe.state & 1) || (lookstrafe->value && mlooking ))
		cmd->sidemove += m_side->value * x;
    else 
	cl.viewangles[YAW] -= m_yaw->value * x;
    
	if ((in_strafe.state & 1)) cmd->upmove += m_forward->value * y;
	else cmd->forwardmove += m_forward->value * y;
      }
      
      x=analog_rx-analog_crx;
      y=analog_cry-analog_ry;
      if (psx_showaxis->value) Com_Printf("Right: (%d %d)\n  ", x, y);
      if ((abs(x)>psx_deadzone->value) || (abs(y)>psx_deadzone->value))
      {
	x<<=1;
	y<<=1;
	x=x*psx_sensitivity->value;
	y=y*psx_sensitivity->value;
	if ( (in_strafe.state & 1) || (lookstrafe->value && mlooking ))
	cmd->sidemove += m_side->value * x;
    else 
	cl.viewangles[YAW] -= m_yaw->value * x;
	
    cl.viewangles[PITCH]+=m_pitch->value*y;
      }

      if (gameport_curr & PSX_R3) tj.value=tj.value|1024;
      else if (tj.value&1024)
      {
	tj.value=tj.value&~1024;
	tj.notvalue|=1024;
      }

      if (gameport_curr & PSX_L3) tj.value=tj.value|2048;
      else if (tj.value&2048)
      {
	tj.value=tj.value&~2048;
	tj.notvalue|=2048;
      }
    }

    if (gameport_ie.ie_Class == PSX_CLASS_MOUSE) 
    {
      gameport_curr = ~PSX_BUTTONS(gameport_ie);

      if (gameport_curr & PSX_LMB) tj.value = tj.value|1;
      else if (tj.value&1)
      {
	tj.value=tj.value&~1;
	tj.notvalue|=1;
      }

      if (gameport_curr & PSX_RMB) tj.value=tj.value|4;
      else if (tj.value&4)
      {
	tj.value=tj.value&~4;
	tj.notvalue|=4;
      }


      x=PSX_MOUSEDX(gameport_ie);
      y=-PSX_MOUSEDY(gameport_ie);
      x<<=2;
      y<<=2;
      x=x*sensitivity->value;
      y=y*sensitivity->value;
      if ( (in_strafe.state & 1) || (lookstrafe->value && mlooking ))
      cmd->sidemove += m_side->value * x;
      else 
      cl.viewangles[YAW] -= m_yaw->value * x;
     
      if (mlooking||freelook->value && !(in_strafe.state & 1))
       cl.viewangles[PITCH]+=m_pitch->value*y;
    }

    gameport_io->io_Command = GPD_READEVENT;
    gameport_io->io_Length = sizeof (struct InputEvent);
    gameport_io->io_Data = &gameport_ie;
    SendIO ((struct IORequest *)gameport_io);
  }
}

void IN_CommandsPsx (void)
{
}


float GetStickMovementPSX(float value, float speed, cvar_t *threshhold, cvar_t *reverse)
{
    float   result = 0.0;

    if(fabs(value) > threshhold->value)
    {
	if(value > 0)
	{
	    result = speed * 64;
	}
	else
	{
	    result = -speed * 64;
	}
    }
    if(reverse->value)
    {
	result = -result;
    }
    return(result);
}

void IN_JoyMovePsx (usercmd_t *cmd)
{
    float   speed, aspeed;
    float   fAxisValue;
    int joy_key;
    if (!gameport_is_open)
    {
	return; 
    }
 
    if ( (in_speed.state & 1) ^ (int)cl_run->value) speed = 2;
	else speed = 1;
    aspeed = speed * cls.frametime;

    GetValuesPsx(cmd);

    fAxisValue= tj.mv0;

    fAxisValue -= 32768;

    fAxisValue /= 32768;

		fAxisValue = (fAxisValue - 0.5)*psx_joyspeed->value;

		if (fabs(fAxisValue)<psx_deadzone2->value) fAxisValue=0;

    cmd->forwardmove -= GetStickMovementPSX(fAxisValue, speed, joy_forwardthreshold, joy_forward_reverse)*psx_joyspeed->value;

    fAxisValue = tj.mv1;

    fAxisValue -= 32768;

    fAxisValue /= 32768;

		fAxisValue = (fAxisValue - 0.5)*psx_joyspeed->value;

		if (fabs(fAxisValue)<psx_deadzone2->value) fAxisValue=0;

    cl.viewangles[YAW] -= (fAxisValue * 0.5 * joy_pitchsensitivity->value) * aspeed * cl_pitchspeed->value;

    if (tj.value&1)
    {
			joy_key = K_JOY1;
			Key_Event(joy_key+3,true,cls.realtime, -1);
    }
    else
    {
			if (tj.notvalue&1)
			{
		  joy_key = K_JOY1;
		    Key_Event(joy_key+3,false,cls.realtime, -1);
			}
    }


    if (tj.value&2)
    {
			joy_key = K_JOY1;
			Key_Event(joy_key,true,cls.realtime, -1);
    }
    else
    {
			if (tj.notvalue&2)
			{
		  joy_key = K_JOY1;
		Key_Event(joy_key,false,cls.realtime, -1);
			}
    }

    if (tj.value&4)
    {
			joy_key = K_JOY1;
			Key_Event(joy_key+2,true,cls.realtime, -1);
    }
    else
    {
			if (tj.notvalue&4)
			{
		  joy_key = K_JOY1;
		Key_Event(joy_key+2,false,cls.realtime, -1);
			}
    }

    if (tj.value&8)
    {
			joy_key = K_JOY1;
			Key_Event(joy_key+1,true,cls.realtime, -1);
    }
    else
    {
			if (tj.notvalue&8)
			{
		  joy_key = K_JOY1;
		Key_Event(joy_key+1,false,cls.realtime, -1);
			}
    }

    if (tj.value&16)
    {
			cmd->sidemove += GetStickMovementPSX(32768, speed, joy_sidethreshold, joy_side_reverse);
    }

    if (tj.value&32)
    {
			cmd->sidemove -= GetStickMovementPSX(32768, speed, joy_sidethreshold, joy_side_reverse);
    }
	if (tj.value&64)
	{ 
	  joy_key = K_JOY1;
	  Key_Event(joy_key+4,true,cls.realtime, -1);
	}
	else
	{
	  if (tj.notvalue&64)
	  {
	    joy_key=K_JOY1;
	    Key_Event(joy_key+4,false,cls.realtime, -1);
	  }
	}
	if (tj.value&128)
	{
	   joy_key = K_JOY1;
	   Key_Event(joy_key+5,true,cls.realtime, -1);
	}
	else
	{
	  if (tj.notvalue&128)
	  {
	    joy_key=K_JOY1;
	    Key_Event(joy_key+5,false,cls.realtime, -1);
	  }
	}
	if (tj.value&256)
	{
	   joy_key = K_JOY1;
	   Key_Event(joy_key+6,true,cls.realtime, -1);
	}
	else
	{
	  if (tj.notvalue&256)
	  {
	    joy_key=K_JOY1;
	    Key_Event(joy_key+6,false,cls.realtime, -1);
	  }
	}
	if (tj.value&512)
	{
	   joy_key = K_JOY1;
	   Key_Event(joy_key+7,true,cls.realtime, -1);
	}
	else
	{
	   if (tj.notvalue&512)
	   {
	     joy_key=K_JOY1;
	     Key_Event(joy_key+7,false,cls.realtime, -1);
	   }
	}
	if (tj.value&1024)
	{
	  joy_key = K_JOY1;
	  Key_Event(joy_key+8,true,cls.realtime, -1);
	}
	else
	{
	  if (tj.notvalue&1024)
	  {
	     joy_key=K_JOY1;
	     Key_Event(joy_key+8,false,cls.realtime, -1);
	  }
	}
	if (tj.value&2048)
	{
	  joy_key = K_JOY1;
	  Key_Event(joy_key+9,true,cls.realtime, -1);
	}
	else
	{
	  if (tj.notvalue&2048)
	  {
	    joy_key=K_JOY1;
	    Key_Event(joy_key+9,false,cls.realtime, -1);
	  }
	}

}

void IN_InitPsx()
{
 ResetPsx();
 tj.mv0=32768;
 tj.mv1=32768;
 psx_sensitivity = Cvar_Get ("psx_sensitivity", "2", CVAR_ARCHIVE);
 psx_deadzone = Cvar_Get("psx_deadzone", "16", CVAR_ARCHIVE);
 psx_showaxis = Cvar_Get("psx_showaxis", "0", 0);
 joy_onlypsx = Cvar_Get("joy_onlypsx","0",0);
 psx_joyspeed = Cvar_Get("psx_joyspeed", "2", CVAR_ARCHIVE);
 psx_deadzone2 = Cvar_Get("psx_deadzone2", "0.2", CVAR_ARCHIVE);
}
