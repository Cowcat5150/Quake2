// vid_null.c -- null video driver to aid porting efforts
// this assumes that one of the refs is statically linked to the executable

#include "../client/client.h"
#include "keys.h"

viddef_t	viddef;				// global video state

refexport_t	re;

refexport_t GetRefAPI (refimport_t rimp);

cvar_t		*vid_gamma;
cvar_t		*vid_ref;			// Name of Refresh DLL loaded
cvar_t		*vid_xpos;			// X coordinate of window position
cvar_t		*vid_ypos;			// Y coordinate of window position
cvar_t		*vid_fullscreen;

void		*reflib_library;
qboolean	reflib_active = 0;

qboolean VID_GetModeInfo( int *width, int *height, int mode );

#define VID_NUM_MODES ( sizeof( vid_modes ) / sizeof( vid_modes[0] ) )

#define	MAXPRINTMSG	4096
void VID_Printf (int print_level, char *fmt, ...)
{
        va_list		argptr;
        char		msg[MAXPRINTMSG];

        va_start (argptr,fmt);
        vsprintf (msg,fmt,argptr);
        va_end (argptr);

        if (print_level == PRINT_ALL)
                Com_Printf ("%s", msg);
        else
                Com_DPrintf ("%s", msg);
}

void VID_Error (int err_level, char *fmt, ...)
{
        va_list		argptr;
        char		msg[MAXPRINTMSG];

        va_start (argptr,fmt);
        vsprintf (msg,fmt,argptr);
        va_end (argptr);

		Com_Error (err_level, "%s", msg);
}

void VID_NewWindow (int width, int height)
{
        viddef.width = width;
        viddef.height = height;
}

struct Mode_Screen * (*GetModeScreen)()=0L;

void VID_FreeReflib(void)
{
  	if (reflib_library)
    {
      dllFreeLibrary(reflib_library);
      memset(&re, 0, sizeof(re));
      reflib_library = 0;
      reflib_active = 0;
      GetModeScreen = 0;
    }
}

qboolean VID_LoadRefresh(char *name)
{
  	refimport_t ri;
    GetRefAPI_t GetRefAPI;
  	void *h;

    if (reflib_active)
    {
      	re.Shutdown();
        VID_FreeReflib();
    }

		Com_Printf( "------- Loading %s -------\n", name );

  	h=(void *)dllLoadLibrary(name,(char *)name);

  	if(!h)
  	{
	    Sys_Error("Failed to load %s\n", name);
      return 0;
  	}
    else reflib_library = h;

		ri.Cmd_AddCommand = Cmd_AddCommand;
		ri.Cmd_RemoveCommand = Cmd_RemoveCommand;
		ri.Cmd_Argc = Cmd_Argc;
		ri.Cmd_Argv = Cmd_Argv;
		ri.Cmd_ExecuteText = Cbuf_ExecuteText;
		ri.Con_Printf = VID_Printf;
		ri.Sys_Error = VID_Error;
		ri.FS_LoadFile = FS_LoadFile;
		ri.FS_FreeFile = FS_FreeFile;
		ri.FS_Gamedir = FS_Gamedir;
		ri.Cvar_Get = Cvar_Get;
		ri.Cvar_Set = Cvar_Set;
		ri.Cvar_SetValue = Cvar_SetValue;
		ri.Vid_GetModeInfo = VID_GetModeInfo;
		ri.Vid_MenuInit = VID_MenuInit;
		ri.Vid_NewWindow = VID_NewWindow;

    if ((GetRefAPI = (void *)dllGetProcAddress(reflib_library,"GetRefAPI")) == 0)
     Com_Error( ERR_FATAL, "dlsym failed on %s", name );

    if ((GetModeScreen = (void *)dllGetProcAddress(reflib_library,"GetModeScreen")) == 0)
      Com_Error( ERR_FATAL, "dlsym failed on ", "GetModeScreen()");

    re = GetRefAPI(ri);

		if (re.api_version != API_VERSION)
		{
			VID_FreeReflib ();
			Com_Error (ERR_FATAL, "%s has incompatible api_version", name);
		}

		if ( re.Init( 0, 0 ) == -1 )
		{
			re.Shutdown();
			VID_FreeReflib ();
			return false;
		}

		Com_Printf( "------------------------------------\n");
		reflib_active = true;
		return true;
}

byte scantokey[128] =
	{
	'`','1','2','3','4','5','6','7',                                 // 7
	'8','9','0','-','=','\\',0,K_INS,                                // f
	'q','w','e','r','t','y','u','i',                                 // 17
	'o','p','[',']',0,K_END,K_KP_DOWNARROW,K_PGDN,                   // 1f
	'a','s','d','f','g','h','j','k',                                 // 27
	'l',';','\'',0,0,K_KP_LEFTARROW,K_KP_5,K_KP_RIGHTARROW,          // 2f
	'<','z','x','c','v','b','n','m',                                 // 37
	',','.',K_KP_SLASH,0,K_KP_DEL,K_HOME,K_KP_UPARROW,K_PGUP,        // 3f
	K_SPACE,K_BACKSPACE,K_TAB,K_KP_ENTER,K_ENTER,K_ESCAPE,K_DEL,0,   // 47
	0,0,K_KP_MINUS,0,K_UPARROW,K_DOWNARROW,K_RIGHTARROW,K_LEFTARROW, // 4f
	K_F1,K_F2,K_F3,K_F4,K_F5,K_F6,K_F7,K_F8,                         // 57
	K_F9,K_F10,0,0,0,0,0,K_F11,                                      // 5f
	K_SHIFT,K_SHIFT,0,K_CTRL,K_ALT,K_ALT,0,0,               // 67
	0,0,0,0,0,0,0,0,                                                 // 6f
	0,0,0,0,0,0,0,0,                                                 // 77
	0,0,0,0,0,0,0,0                                                  // 7f
	};

int MapKey (int key)
{
    return scantokey[key];
}

void VID_Restart_f (void)
{
	vid_ref->modified = true;
}

typedef struct vidmode_s
{
    const char *description;
    int         width, height;
    int         mode;
} vidmode_t;

vidmode_t vid_modes[] =
{
    { "Mode 0: 320x240",   320, 240,   0 },
    { "Mode 1: 400x300",   400, 300,   1 },
    { "Mode 2: 512x384",   512, 384,   2 },
    { "Mode 3: 640x480",   640, 480,   3 },
    { "Mode 4: 800x600",   800, 600,   4 },
    { "Mode 5: 960x720",   960, 720,   5 },
    { "Mode 6: 1024x768",  1024, 768,  6 },
    { "Mode 7: 1152x864",  1152, 864,  7 },
    { "Mode 8: 1280x960",  1280, 960, 8 },
    { "Mode 9: 1600x1200", 1600, 1200, 9 }
};
#define VID_NUM_MODES ( sizeof( vid_modes ) / sizeof( vid_modes[0] ) )

qboolean VID_GetModeInfo( int *width, int *height, int mode )
{
    if ( mode < 0 || mode >= VID_NUM_MODES )
        return false;

    *width  = vid_modes[mode].width;
    *height = vid_modes[mode].height;

    return true;
}


void	VID_Init (void)
{
    vid_ref = Cvar_Get ("vid_ref", "soft", CVAR_ARCHIVE);
    vid_xpos = Cvar_Get ("vid_xpos", "0", CVAR_ARCHIVE);
    vid_ypos = Cvar_Get ("vid_ypos", "0", CVAR_ARCHIVE);
    vid_fullscreen = Cvar_Get ("vid_fullscreen", "0", CVAR_ARCHIVE);
    vid_gamma = Cvar_Get( "vid_gamma", "0.5", CVAR_ARCHIVE );
 
	  Cmd_AddCommand ("vid_restart", VID_Restart_f);

    VID_CheckChanges();
}

void	VID_Shutdown (void)
{
  	if (reflib_active)
    {
     	re.Shutdown();
      VID_FreeReflib();
    }
}

void	VID_CheckChanges (void)
{
	char name[100];
	cvar_t *sw_mode;

	if ( vid_ref->modified )
	{
		S_StopAllSounds();
	}

	while (vid_ref->modified)
	{
		/*
		** refresh has changed
		*/
		vid_ref->modified = false;
		vid_fullscreen->modified = true;
		cl.refresh_prepped = false;
		cls.disable_screen = true;

		sprintf( name, "ref_%s.dll", vid_ref->string );
		if ( !VID_LoadRefresh( name ) )
		{
			if ( strcmp (vid_ref->string, "soft") == 0 ||
				strcmp (vid_ref->string, "softx") == 0 ) {
			Com_Printf("Refresh failed\n");
				sw_mode = Cvar_Get( "sw_mode", "0", 0 );
				if (sw_mode->value != 0) {
			Com_Printf("Trying mode 0\n");
					Cvar_SetValue("sw_mode", 0);
					if ( !VID_LoadRefresh( name ) )
						Com_Error (ERR_FATAL, "Couldn't fall back to software refresh!");
				} else
					Com_Error (ERR_FATAL, "Couldn't fall back to software refresh!");
			}

			Cvar_Set( "vid_ref", "soft" );

			/*
			** drop the console if we fail to load a refresh
			*/
			if ( cls.key_dest != key_console )
			{
				Con_ToggleConsole_f();
			}
		}
		cls.disable_screen = false;
	}
}


