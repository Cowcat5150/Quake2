#include "../client/client.h"
#include "../client/snd_loc.h"
#include "dll.h"
#include "snddll.h"

/*
** File:    snd_amiga.c
** Date:    25-Dec-2001
** Purpose: SndDll glue code for loading external sound systems
** Author:  Hans-Joerg Frieden
**
*/

void *	    sound_library;
qboolean    soundlib_active;
sndexport_t se;
cvar_t *    s_lib = 0;

extern void VID_Printf (int print_level, char *fmt, ...);
extern void VID_Error (int err_level, char *fmt, ...);

void SND_CheckChanges(void)
{
	if (s_lib && s_lib->modified == true)
	{
		extern void CL_Snd_Restart_f(void);
		CL_Snd_Restart_f();
		s_lib->modified = false;
	}
}

void SND_FreeSoundlib(void)
{
	if (sound_library)
		dllFreeLibrary(sound_library);

	memset (&se, 0, sizeof(se));
	soundlib_active = false;
}

qboolean SND_LoadSoundlib(char *name)
{
	sndimport_t	si;
	GetSndAPI_t	GetSndAPI;

	if (soundlib_active)
	{
		se.Shutdown(&dma);
		SND_FreeSoundlib();
	}

	Com_Printf( "------- Loading %s -------\n", name );

	if ( (sound_library = dllLoadLibrary(name, name)) == 0)
	{
		Com_Printf ("LoadLibrary \"%s\" failed\n", name);
		return false;
	}

	si.Cmd_AddCommand    = Cmd_AddCommand;
	si.Cmd_RemoveCommand = Cmd_RemoveCommand;
	si.Cmd_Argc = Cmd_Argc;
	si.Cmd_Argv = Cmd_Argv;
	si.Cmd_ExecuteText = Cbuf_ExecuteText;
	si.Con_Printf  = VID_Printf;
	si.Sys_Error   = VID_Error;
	si.FS_LoadFile = FS_LoadFile;
	si.FS_FreeFile = FS_FreeFile;
	si.FS_Gamedir  = FS_Gamedir;
	si.Cvar_Get = Cvar_Get;
	si.Cvar_Set = Cvar_Set;
	si.Cvar_SetValue = Cvar_SetValue;

	if ( ( GetSndAPI = (void *) dllGetProcAddress( sound_library, "GetSndAPI" ) ) == 0 )
		Com_Error( ERR_FATAL, "GetProcAddress failed on %s", name );

	se = GetSndAPI( si );

	if (se.api_version != SND_API_VERSION)
	{
		SND_FreeSoundlib ();
		Com_Error (ERR_FATAL, "%s has incompatible snd_api_version", name);
	}

	if ( se.Init( &dma ) == -1 )
	{
		se.Shutdown(&dma);
		SND_FreeSoundlib ();
		return false;
	}

	Com_Printf( "------------------------------------\n");
	soundlib_active = true;
	return true;
}

qboolean SNDDMA_Init(void)
{
	char name[100];

	if (!s_lib)
		s_lib = Cvar_Get("s_lib", "paula", CVAR_ARCHIVE);

	Com_sprintf(name, sizeof(name), "snd_%s.dll", s_lib->string);

	return SND_LoadSoundlib(name);
}

int SNDDMA_GetDMAPos(void)
{
	return se.GetDMAPos(&dma);
}

void SNDDMA_Shutdown(void)
{
	se.Shutdown(&dma);
	SND_FreeSoundlib();
}

void SNDDMA_BeginPainting (void)
{
	if (se.BeginPainting)
		se.BeginPainting(&dma);
}

void SNDDMA_Submit(void)
{
	if (se.Submit)
		se.Submit(&dma);
}
