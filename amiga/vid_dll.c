#include "../client/client.h"
#include "dll.h"

#pragma pack(push,2)

#include <intuition/screens.h>
#include <intuition/intuition.h>
#include <proto/intuition.h>
#include <proto/dos.h>

#pragma pack(push,2)

// Structure containing functions exported from refresh DLL
refexport_t	re;

cvar_t		*vid_gamma;
cvar_t		*vid_ref;		// Name of Refresh DLL loaded
cvar_t		*vid_xpos;		// X coordinate of window position
cvar_t		*vid_ypos;		// Y coordinate of window position
cvar_t		*vid_fullscreen;

// Global variables used internally by this module
viddef_t	viddef;			// global video state; used by other modules
void *		reflib_library;		// Handle to refresh DLL
qboolean	reflib_active = 0;

struct Window	*win;  		//Cowcat
extern void	MouseHandler(); // Cowcat Windowmode

/*
==========================================================================

DLL GLUE

==========================================================================
*/

#define MAXPRINTMSG	4096

DLLFUNC void VID_Printf (int print_level, char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];
	static qboolean inupdate;

	va_start (argptr,fmt);
	vsprintf (msg,fmt,argptr);
	va_end	 (argptr);

	if (print_level == PRINT_ALL)
	{
		Com_Printf ("%s", msg);
	}

	else if ( print_level == PRINT_DEVELOPER )
	{
		Com_DPrintf ("%s", msg);
	}

	else if ( print_level == PRINT_ALERT )
	{
		Com_DPrintf("ALERT: %s", msg);
	}
}

DLLFUNC void VID_Error (int err_level, char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];
	static qboolean inupdate;

	va_start (argptr,fmt);
	vsprintf (msg,fmt,argptr);
	va_end (argptr);
	
	Com_Error (err_level,"%s", msg);
}

//==========================================================================

byte scantokey[128] = {
	'`','1','2','3','4','5','6','7',				 // 7
	'8','9','0','-','=','\\',0,K_INS,				 // f
	'q','w','e','r','t','y','u','i',				 // 17
	'o','p','[',']',0,K_END,K_KP_DOWNARROW,K_PGDN,			 // 1f
	'a','s','d','f','g','h','j','k',				 // 27
	'l',';','\'',0,0,K_KP_LEFTARROW,K_KP_5,K_KP_RIGHTARROW,		 // 2f
	'<','z','x','c','v','b','n','m',				 // 37
	',','.',K_KP_SLASH,0,K_KP_DEL,K_HOME,K_KP_UPARROW,K_PGUP,	 // 3f
	K_SPACE,K_BACKSPACE,K_TAB,K_KP_ENTER,K_ENTER,K_ESCAPE,K_DEL,0,	 // 47
	0,0,K_KP_MINUS,0,K_UPARROW,K_DOWNARROW,K_RIGHTARROW,K_LEFTARROW, // 4f
	K_F1,K_F2,K_F3,K_F4,K_F5,K_F6,K_F7,K_F8,			 // 57
	K_F9,K_F10,0,0,0,0,0,K_F11,					 // 5f
	K_SHIFT,K_SHIFT,0,K_CTRL,K_ALT,K_ALT,0,0,			 // 67
	0,0,0,0,0,0,0,0,						 // 6f
	0,0,0,0,0,0,0,0,						 // 77
	0,0,0,0,0,0,0,0							 // 7f
};

/*
=======
MapKey

Map from rawkey to quake keynums
=======
*/

int MapKey (int key)
{
	return scantokey[key];
}

/*
============
VID_Restart_f

Console command to re-start the video mode and refresh DLL. We do this
simply by setting the modified flag for the vid_ref variable, which will
cause the entire video mode and refresh DLL to be reset on the next frame.
============
*/

void VID_Restart_f (void)
{	
	vid_ref->modified = true;	

}

void VID_Front_f( void )
{
}

/*
** VID_GetModeInfo
*/

typedef struct vidmode_s
{
	const char	*description;
	int	    	width, height;
	int	    	mode;

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
	{ "Mode 8: 1280x960",  1280, 960,  8 },
	{ "Mode 9: 1600x1200", 1600, 1200, 9 },

	{ "Mode 10: 512x288",	512, 288, 10 }, // widescreen modes - Cowcat
	{ "Mode 11: 640x360",	640, 360, 11 },
	{ "Mode 12: 768x432",	768, 432, 12 },
	{ "Mode 13: 848x480",	848, 480, 13 },
	{ "Mode 14: 960x540",	960, 540, 14 },
	{ "Mode 15: 1024x576",	1024, 576, 15 },
	{ "Mode 16: 1152x648",	1152, 648, 16 },
	{ "Mode 17: 1280x720",	1280, 720, 17 },
	{ "Mode 18: 1600x900",	1600, 900, 18 }
	
};

#define VID_NUM_MODES ( sizeof( vid_modes ) / sizeof( vid_modes[0] ) )

DLLFUNC qboolean VID_GetModeInfo( int *width, int *height, int mode )
{
	if ( mode < 0 || mode >= VID_NUM_MODES )
		return false;

	*width	= vid_modes[mode].width;
	*height = vid_modes[mode].height;

	return true;
}

/*
** VID_NewWindow
*/

DLLFUNC void VID_NewWindow ( int width, int height)
{
	viddef.width  = width;
	viddef.height = height;

	cl.force_refdef = true;		// can't use a paused refdef
}

void VID_FreeReflib (void)
{
	dllFreeLibrary( reflib_library);
	memset (&re, 0, sizeof(re));
	reflib_library = NULL;
	reflib_active  = false;
}

struct Window * (*GetWindowHandle)(void);
struct MsgPort *g_pMessagePort;

/*
==============
VID_LoadRefresh
==============
*/

qboolean VID_LoadRefresh( char *name )
{
	refimport_t	ri;
	GetRefAPI_t	GetRefAPI;

	if ( reflib_active )
	{
		re.Shutdown();
		VID_FreeReflib ();
	}
	
	Com_Printf( "------- Loading %s -------\n", name );

	if ( ( reflib_library = dllLoadLibrary( name, name ) ) == 0 )
	{
		Com_Printf( "LoadLibrary(\"%s\") failed\n", name );
		return false;
	}

	Com_Printf("... loaded %s\n", name);

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

	if ( ( GetRefAPI = (void *) dllGetProcAddress( reflib_library, "GetRefAPI" ) ) == 0 )
		Com_Error( ERR_FATAL, "GetProcAddress failed on %s", name );
	
	if ( ( GetWindowHandle = (void *) dllGetProcAddress(reflib_library, "GetWindowHandle")) == 0)
		Com_Error ( ERR_FATAL, "GetProcAddress(GetWindowHandle) failed on %s", name);

	if (GetRefAPI)
		re = GetRefAPI( ri );  
	
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
 
	if (GetWindowHandle)
	{		
		//struct Window *win = GetWindowHandle(); //Cowcat win global

		win = GetWindowHandle();

		if (!win)
		{		
			re.Shutdown();
			VID_FreeReflib();
			return false;
		}

		ModifyIDCMP(win, IDCMP_RAWKEY|IDCMP_MOUSEBUTTONS|IDCMP_DELTAMOVE|IDCMP_MOUSEMOVE);
		//ModifyIDCMP(win, IDCMP_RAWKEY|IDCMP_MOUSEBUTTONS);
	    
		//ReportMouse(TRUE, win);
		win->Flags |= WFLG_REPORTMOUSE;
		
		g_pMessagePort = win->UserPort;
	}

	Com_Printf( "------------------------------------\n");
	reflib_active = true;

//======
//PGM
	vidref_val = VIDREF_OTHER;

	if(vid_ref)
	{
		if(!strcmp(vid_ref->string,"glnolru"))
			vidref_val = VIDREF_GL; //2; why 2 ? - Cowcat

		else if(!strcmp (vid_ref->string, "gl"))
			vidref_val = VIDREF_GL;

		else if(!strcmp(vid_ref->string, "soft"))
			vidref_val = VIDREF_SOFT;
	}
//PGM
//======
	
	return true;
}

/*
============
VID_CheckChanges

This function gets called once just before drawing each frame, and it's sole purpose in life
is to check to see if any of the video mode parameters have changed, and if they have to
update the rendering DLL and/or video mode to match.
============
*/

void VID_CheckChanges (void)
{
	extern	void SND_CheckChanges(void);
	char	name[100];

	if ( vid_ref->modified )
	{
		cl.force_refdef = true;		// can't use a paused refdef
		S_StopAllSounds();
		MouseHandler();			// Cowcat windowmode MouseHandler
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

		Com_sprintf( name, sizeof(name), "ref_%s.dll", vid_ref->string );

		if ( !VID_LoadRefresh( name ) )
		{
			printf("VID_CheckChanges !VID_Loadrefresh\n");

			if ( strcmp (vid_ref->string, "soft") == 0 )
				Com_Error (ERR_FATAL, "Could not fall back to software refresh!");

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

	// Check for a change in the sound dll
	SND_CheckChanges();	
}

/*
============
VID_Init
============
*/

void VID_Init (void)
{		
	/* Create the video variables so we know how to start the graphics drivers */
	vid_ref = Cvar_Get ("vid_ref", "soft", CVAR_ARCHIVE);
	vid_xpos = Cvar_Get ("vid_xpos", "90", CVAR_ARCHIVE); // was 3
	vid_ypos = Cvar_Get ("vid_ypos", "60", CVAR_ARCHIVE); // was 22
	vid_fullscreen = Cvar_Get ("vid_fullscreen", "0", CVAR_ARCHIVE);
	vid_gamma = Cvar_Get( "vid_gamma", "1", CVAR_ARCHIVE );

	/* Add some console commands that we want to handle */
	Cmd_AddCommand ("vid_restart", VID_Restart_f);
	Cmd_AddCommand ("vid_front", VID_Front_f);

	/* Start the graphics mode and load refresh DLL */

	VID_CheckChanges();
}

/*
============
VID_Shutdown
============
*/

void VID_Shutdown (void)
{
	if ( reflib_active )
	{
		re.Shutdown ();
		VID_FreeReflib ();
	}
}



