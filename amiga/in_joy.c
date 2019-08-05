#include "../client/client.h"
#include "in.h"

#ifdef __VBCC__
#pragma amiga-align
#elif defined(WARPUP)
#pragma pack(push,2)
#endif

#include <libraries/lowlevel.h>
#include <clib/lowlevel_protos.h>
#include <clib/exec_protos.h>
#include <proto/lowlevel.h>
#include <proto/exec.h>

#ifdef __VBCC__
#pragma default-align
#elif defined(WARPUP)
#pragma pack(push,2)
#endif

ULONG State[4];

ULONG in_ack0 = -1, in_ack1 = -1;

struct Library *LowLevelBase = 0;

extern cvar_t  *in_joystick;

cvar_t  *joy_name;
cvar_t  *joy_advanced;
cvar_t  *joy_advaxisx;
cvar_t  *joy_advaxisy;
cvar_t  *joy_advaxisz;
cvar_t  *joy_advaxisr;
cvar_t  *joy_advaxisu;
cvar_t  *joy_advaxisv;
cvar_t  *joy_forwardthreshold;
cvar_t  *joy_sidethreshold;
cvar_t  *joy_upthreshold;
cvar_t  *joy_pitchthreshold;
cvar_t  *joy_yawthreshold;
cvar_t  *joy_pitchsensitivity;
cvar_t  *joy_yawsensitivity;

cvar_t  *joy_side_reverse;
cvar_t  *joy_up_reverse;
cvar_t  *joy_forward_reverse;
extern cvar_t *joy_onlypsx;

struct ForceTypes
{
  char *Name;
  ULONG Type;
};

struct ForceTypes fTypes[] =
{
  "GAMECTRL", SJA_TYPE_GAMECTLR,
  "MOUSE",    SJA_TYPE_MOUSE,
  "JOYSTICK", SJA_TYPE_JOYSTK,
  "AUTOSENSE",SJA_TYPE_AUTOSENSE,
  NULL,       0,
};

struct JoyStickDesc
{
    int axis;
    int mv0;
    int mv1;
    int value,notvalue;
};

struct JoyStickDesc jd;
ULONG Port;

ULONG StringTypeToType(char *type)
{
  int i = 0;
  while (i)
  {
      if (0 == strcmp(type, fTypes[i].Name)) return fTypes[i].Type;
      i++;
  }
  return SJA_TYPE_AUTOSENSE;
}

void IN_ResetStates (void)
{
  ULONG Port;
  for (Port = 0; Port < 4; ++Port)
  {
      State[Port] = (ULONG)JP_TYPE_NOTAVAIL;
      SetJoyPortAttrs(Port,
	  SJA_Type, SJA_TYPE_AUTOSENSE,
      TAG_DONE);
  }
}

int IN_InitStates ()
{
 ULONG Port = ((ULONG)(NULL));
 int n=0;

 for (Port=0;Port<4; Port++)
 {
  State[ Port ] = (ULONG)(ReadJoyPort( ((ULONG)(Port)) ));
  if ((State[Port]!=JP_TYPE_NOTAVAIL)&&(!(State[Port]&JP_TYPE_MOUSE))) return Port;
 }
 jd.mv0=32768;
 jd.mv1=32768;
 return -1;
}

int MyInitJoy()
{
   LowLevelBase = (struct Library *)OpenLibrary("lowlevel.library",0);
   if (LowLevelBase)
   {
    IN_ResetStates();
    return IN_InitStates();
   }
   return -1;
}

int stick=1;

void IN_StartupJoystick (void) 
{ 
    int         numdevs;
    cvar_t      *cv;

    cv = Cvar_Get ("in_initjoy", "1", CVAR_NOSET);
    if ( !cv->value ) return;
 
    if (!joy_onlypsx) joy_onlypsx = Cvar_Get("joy_onlypsx", "0", 0);
    if (joy_onlypsx->value) return;

    stick=1;

    if ((numdevs = MyInitJoy()) == -1)
    {
			if (LowLevelBase) CloseLibrary(LowLevelBase);
			return;
    }
    else Port=numdevs;

    //Com_Printf ("\nJoystick detected\n\n");
  	cv = Cvar_Get("joy_force0", "AUTOSENSE", CVAR_NOSET);
  	if (cv->string)
	  {
	      SetJoyPortAttrs(0,
			  SJA_Type,   StringTypeToType(cv->string),
	      TAG_DONE);
	      //Com_Printf("Joystick Port 0 is %s\n", cv->string);
  	}

  	cv = Cvar_Get("joy_force1", "AUTOSENSE", CVAR_NOSET);
  	if (cv->string)
	  {
  	    SetJoyPortAttrs(1,
			  SJA_Type,   StringTypeToType(cv->string),
  	    TAG_DONE);
    	  //Com_Printf("Joystick Port 1 is %s\n", cv->string);
	  }

  	cv = Cvar_Get("joy_force2", "AUTOSENSE", CVAR_NOSET);
  	if (cv->string)
	  {
	      SetJoyPortAttrs(2,
			  SJA_Type,   StringTypeToType(cv->string),
  	    TAG_DONE);
    	  //Com_Printf("Joystick Port 2 is %s\n", cv->string);
	  }

  	cv = Cvar_Get("joy_force3", "AUTOSENSE", CVAR_NOSET);
  	if (cv->string)
	  {
  	    SetJoyPortAttrs(3,
			  SJA_Type,   StringTypeToType(cv->string),
    	  TAG_DONE);
      	//Com_Printf("Joystick Port 3 is %s\n", cv->string);
	  }
}


int GetValues()
{
 ULONG Button=0;
 ULONG Direction=0;
 ULONG type;

 if ( ( State[ Port ] = ReadJoyPort(Port)) != JP_TYPE_NOTAVAIL )
 {
   Button=(State[ Port ]) & (JP_BUTTON_MASK);
   Direction = (State[ Port ]) & (JP_DIRECTION_MASK);
   jd.notvalue=0;

   type=State[Port]&JP_TYPE_MASK;

   if ((type&JP_TYPE_JOYSTK)&&(type&JP_TYPE_GAMECTLR)) type=JP_TYPE_GAMECTLR;

   if (((type&JP_TYPE_JOYSTK)||(type&JP_TYPE_GAMECTLR))==0) type=JP_TYPE_JOYSTK;

   switch(type)
   {
    case JP_TYPE_GAMECTLR:
    {
     if (stick) stick=0;
     if (Button&JPF_BUTTON_RED)
     {
      jd.value=jd.value|1;
     }
     else if (jd.value&1)
     {
      jd.value=jd.value&~1;
      jd.notvalue|=1;
     }
     if (Button&JPF_BUTTON_BLUE)
     {
      jd.value=jd.value|2;
     }
     else if (jd.value&2)
     {
      jd.value=jd.value&~2;
      jd.notvalue|=2;
     }
     if (Button&JPF_BUTTON_YELLOW)
     {
      jd.value=jd.value|4;
     }
     else if (jd.value&4)
     {
      jd.value=jd.value&~4;
      jd.notvalue|=4;
     }
     if (Button&JPF_BUTTON_GREEN)
     {
      jd.value=jd.value|8;
     }
     else if (jd.value&8)
     {
      jd.value=jd.value&~8;
      jd.notvalue|=8;
     }
     if (Button&JPF_BUTTON_FORWARD)
     {
      jd.value=jd.value|16;
     }
     else if (jd.value&16)
     {
      jd.value=jd.value&~16;
      jd.notvalue|=16;
     }
     if (Button&JPF_BUTTON_REVERSE)
     {
      jd.value=jd.value|32;
     }
     else if (jd.value&32)
     {
      jd.value=jd.value&~32;
      jd.notvalue|=32;
     }
     if (Button&JPF_BUTTON_PLAY)
     {
      jd.value=jd.value|64;
     }
     else if (jd.value&64)
     {
      jd.value=jd.value&~64;
      jd.notvalue|=64;
     }

     if (Direction&JPF_JOY_UP)
     {
      jd.mv0=0;
     }
     else if (Direction&JPF_JOY_DOWN)
     {
      jd.mv0=65536;
     }
     else
     {
      jd.mv0=32768;
     }

     if (Direction&JPF_JOY_LEFT)
     {
      jd.mv1=0;
     }
     else if (Direction&JPF_JOY_RIGHT)
     {
      jd.mv1=65536;
     }
     else
     {
      jd.mv1=32768;
     }


    }

    break;

    case JP_TYPE_JOYSTK :
    {
     if (Button&JPF_BUTTON_RED)
     {
      jd.value=jd.value|1;
     }
     else if (jd.value&1)
     {
      jd.value=jd.value&~1;
      jd.notvalue|=1;
     }

     if (Direction&JPF_JOY_UP)
     {
      jd.mv0=0;
     }
     else if (Direction&JPF_JOY_DOWN)
     {
      jd.mv0=65536;
     }
     else
     {
      jd.mv0=32768;
     }

     if (Direction&JPF_JOY_LEFT)
     {
      jd.mv1=0;
     }
     else if (Direction&JPF_JOY_RIGHT)
     {
      jd.mv1=65536;
     }
     else
     {
      jd.mv1=32768;
     }
    }

    break;

   }
   return 1;
  }
  else return 0;
 }


void IN_ShutdownJoystick(void)
{
  if (LowLevelBase)
  {
   if ( !( ((ULONG)(in_ack1)) ) )
   {
    (SystemControl( ((ULONG)(SCON_RemCreateKeys)), ((ULONG)(1)), ((ULONG)(TAG_END)) ));
   }

   if ( !( ((ULONG)(in_ack0)) ) )
   {
    (SystemControl( ((ULONG)(SCON_RemCreateKeys)), ((ULONG)(0)), ((ULONG)(TAG_END)) ));
   }

   IN_ResetStates();
   if (LowLevelBase) CloseLibrary(LowLevelBase);
  }
}

float GetStickMovement(float value, float speed, cvar_t *threshhold, cvar_t *reverse)
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

void IN_JoyMove (usercmd_t *cmd)
{
    float   speed, aspeed;
    int   fAxisValue;
    int joy_key;

    if (!LowLevelBase) return;

    if ( (in_speed.state & 1) ^ (int)cl_run->value) speed = 2;
    else speed = 1;

    aspeed = speed * cls.frametime;

    if (!(GetValues()));

    fAxisValue= jd.mv0;

    fAxisValue -= 32768;

    fAxisValue /= 32768;

    cmd->forwardmove -= GetStickMovement(fAxisValue, speed, joy_forwardthreshold, joy_forward_reverse);

    fAxisValue = jd.mv1;

    fAxisValue -= 32768;

    fAxisValue /= 32768;

    cl.viewangles[YAW] -= (fAxisValue * 0.5 * joy_pitchsensitivity->value) * aspeed * cl_pitchspeed->value;

    if (jd.value&1)
    {
			joy_key = K_JOY1;
			Key_Event(joy_key+3,true,cls.realtime, -1);
    }
    else
    {
			if (jd.notvalue&1)
			{
	  	  joy_key = K_JOY1;
		    Key_Event(joy_key+3,false,cls.realtime, -1);
			}
    }

    if (stick) return;

    if (jd.value&2)
    {
			joy_key = K_JOY1;
			Key_Event(joy_key,true,cls.realtime, -1);
    }
    else
    {
			if (jd.notvalue&2)
			{
	  	  joy_key = K_JOY1;
	    	Key_Event(joy_key,false,cls.realtime, -1);
			}
    }

    if (jd.value&4)
    {
			joy_key = K_JOY1;
			Key_Event(joy_key+2,true,cls.realtime, -1);
    }
    else
    {
			if (jd.notvalue&4)
			{
	  	  joy_key = K_JOY1;
	  	  Key_Event(joy_key+2,false,cls.realtime, -1);
			}
    }

    if (jd.value&8)
    {
			joy_key = K_JOY1;
			Key_Event(joy_key+1,true,cls.realtime, -1);
    }
    else
    {
			if (jd.notvalue&8)
			{
	  	  joy_key = K_JOY1;
	    	Key_Event(joy_key+1,false,cls.realtime, -1);
			}
    }

    if (jd.value&16)
    {
			cmd->sidemove += GetStickMovement(32768, speed, joy_sidethreshold, joy_side_reverse);
    }

    if (jd.value&32)
    {
			cmd->sidemove -= GetStickMovement(32768, speed, joy_sidethreshold, joy_side_reverse);
    }
}

void IN_Commands (void)
{
}

void IN_InitJoy()
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

}
