#include "/client/client.h"
#include "in.h"
#include <clib/exec_protos.h>

#include <proto/exec.h>

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

cvar_t  *joy_name;
cvar_t  *joy_advaxisx;
cvar_t  *joy_advaxisy;
cvar_t  *joy_advaxisz;
cvar_t  *joy_advaxisr;
cvar_t  *joy_advaxisu;
cvar_t  *joy_advaxisv;
cvar_t  *in_joystick;
cvar_t  *pc_deadzone;
cvar_t  *pc_joyspeed;

#include "WesSupport_protos.h"

struct Library *WesSupportBase;
int WesStick;
struct WES_JoyData wes_joydata;

struct JoyStickDesc
{
    int axis;
    int mv0;
    int mv1;
    int value,notvalue;
};

static struct JoyStickDesc tj;

   
void IN_ShutdownPC(void)
{ 
	if (WesSupportBase)
  {
  	WES_CloseStick(WES_JOYSTICK_PC);
		CloseLibrary(WesSupportBase);
    WesSupportBase=0;
  }
} 

void GetValuesPC(usercmd_t *cmd)
{
	WES_ReadStick(WES_JOYSTICK_PC,&wes_joydata);

  tj.mv0=wes_joydata.x1;
  tj.mv1=wes_joydata.y1;
  if (wes_joydata.button1) tj.value=tj.value|2;
  else if (tj.value&2)
  {
    tj.value=tj.value&~2;
    tj.notvalue|=2;
  }
  if (wes_joydata.button2) tj.value=tj.value|4;
  else if (tj.value&4)
  {
    tj.value=tj.value&~4;
    tj.notvalue|=4;
  }
  if (wes_joydata.button3) tj.value=tj.value|1;
  else if (tj.value&1)
  {
    tj.value=tj.value&~1;
    tj.notvalue|=1;
  }
  if (wes_joydata.button4) tj.value=tj.value|8;
  else if (tj.value&8)
  {
    tj.value=tj.value&~8;
    tj.notvalue|=8;
  }
}

void IN_CommandsPC (void)
{
}


float GetStickMovementPC(float value, float speed, cvar_t *threshhold, cvar_t *reverse)
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

void IN_JoyMovePC (usercmd_t *cmd)
{
    float   speed, aspeed;
    float   fAxisValue;
    int joy_key;
 
    if (!WesSupportBase) return;

    if ( (in_speed.state & 1) ^ (int)cl_run->value) speed = 2;
  	else speed = 1;
    aspeed = speed * cls.frametime;

    GetValuesPC(cmd);

    fAxisValue= tj.mv1*2;
		fAxisValue /=65536;

		fAxisValue = (fAxisValue - 0.5)*pc_joyspeed->value;

		if (fabs(fAxisValue)<pc_deadzone->value) fAxisValue=0;

    cmd->forwardmove -= GetStickMovementPC(fAxisValue, speed, joy_forwardthreshold, joy_forward_reverse)*pc_joyspeed->value;

    fAxisValue = tj.mv0*2;
  
    fAxisValue /= 65536;

    fAxisValue = (fAxisValue - 0.5)*pc_joyspeed->value;

    if (fabs(fAxisValue)<pc_deadzone->value) fAxisValue=0;

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
			cmd->sidemove += GetStickMovementPC(32768, speed, joy_sidethreshold, joy_side_reverse);
    }

    if (tj.value&32)
    {
			cmd->sidemove -= GetStickMovementPC(32768, speed, joy_sidethreshold, joy_side_reverse);
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

void IN_InitPC()
{
 WesSupportBase = (struct Library *)OpenLibrary((UBYTE *)"WesSupport.library",0);
 if (WesSupportBase) WesStick=WES_OpenStick(WES_JOYSTICK_PC,"Quake2");
 tj.mv0=16384;
 tj.mv1=16384;
 if (WesSupportBase)
 {
    in_joystick             = Cvar_Get ("in_joystick",              "0",        CVAR_ARCHIVE);
    joy_name                = Cvar_Get ("joy_name",                 "joystick", 0);
    joy_advanced            = Cvar_Get ("joy_advanced",             "0",        0);
    joy_advaxisx            = Cvar_Get ("joy_advaxisx",             "0",        0);
    joy_advaxisy            = Cvar_Get ("joy_advaxisy",             "0",        0);
    joy_advaxisz            = Cvar_Get ("joy_advaxisz",             "0",        0);
    joy_advaxisr            = Cvar_Get ("joy_advaxisr",             "0",        0);
    joy_advaxisu            = Cvar_Get ("joy_advaxisu",             "0",        0);
    joy_advaxisv            = Cvar_Get ("joy_advaxisv",             "0",        0);
    joy_forwardthreshold    = Cvar_Get ("joy_forwardthreshold",     "0.15",     0);
    joy_sidethreshold       = Cvar_Get ("joy_sidethreshold",        "0.15",     0);
    joy_upthreshold         = Cvar_Get ("joy_upthreshold",          "0.15",     0);
    joy_pitchthreshold      = Cvar_Get ("joy_pitchthreshold",       "0.15",     0);
    joy_yawthreshold        = Cvar_Get ("joy_yawthreshold",         "0.15",     0);
    joy_pitchsensitivity    = Cvar_Get ("joy_pitchsensitivity",     "1",        0);
    joy_yawsensitivity      = Cvar_Get ("joy_yawsensitivity",       "-1",       0);
    joy_up_reverse          = Cvar_Get ("joy_up_reverse",           "0",        CVAR_ARCHIVE);
    joy_forward_reverse     = Cvar_Get ("joy_forward_reverse",      "0",        CVAR_ARCHIVE);
    joy_side_reverse        = Cvar_Get ("joy_side_reverse",         "0",        CVAR_ARCHIVE);
 		pc_deadzone							= Cvar_Get ("pc_deadzone","0.2",CVAR_ARCHIVE);
 		pc_joyspeed							= Cvar_Get ("pc_joyspeed","2",CVAR_ARCHIVE);
 }
}
