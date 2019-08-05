#ifndef SND_DLL_H
#define SND_DLL_H

/*
** File:    snddll.h
** Date:    25-Dec-2001
** Purpose: Definitions for putting sound system specific functionality
** 	    into an external, run-time loadable DLL.
** Author:  Hans-Jörg Frieden, Hyperion Entertainment.
*/

// Cowcat  //Thanks Phx!

#ifdef WOS
#define DLLFUNC __saveds
#else
#define DLLFUNC
#endif

////

typedef struct
{
    	void	(*Sys_Error) (int err_level, char *str, ...);

    	void    (*Cmd_AddCommand) (char *name, void(*cmd)(void));
    	void    (*Cmd_RemoveCommand) (char *name);
    	int     (*Cmd_Argc) (void);
    	char   	*(*Cmd_Argv) (int i);
    	void    (*Cmd_ExecuteText) (int exec_when, char *text);

    	void    (*Con_Printf) (int print_level, char *str, ...);

    	// files will be memory mapped read only
    	// the returned buffer may be part of a larger pak file,
    	// or a discrete file from anywhere in the quake search path
    	// a -1 return means the file does not exist
    	// NULL can be passed for buf to just determine existance
    	int     (*FS_LoadFile) (char *name, void **buf);
    	void    (*FS_FreeFile) (void *buf);

    	// gamedir will be the current directory that generated
    	// files should be stored to, ie: "f:\quake\id1"
    	char    *(*FS_Gamedir) (void);

    	cvar_t  *(*Cvar_Get) (char *name, char *value, int flags);
    	cvar_t  *(*Cvar_Set)( char *name, char *value );
    	void     (*Cvar_SetValue)( char *name, float value );

} sndimport_t;

#define SND_API_VERSION	1

typedef struct
{
    	int		api_version;

    	qboolean 	(*Init)(dma_t *dma);
    	void		(*Shutdown)(dma_t *dma);

    	int		(*GetDMAPos)(dma_t *dma);
    	void		(*BeginPainting)(dma_t *dma); 	// may be null, not called in that case
    	void 		(*Submit)(dma_t *dma); 		// may be null, not called in that case

} sndexport_t;

typedef sndexport_t     (*GetSndAPI_t) (sndimport_t);

#endif
