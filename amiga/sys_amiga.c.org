#include <stdio.h>
#include "../qcommon/qcommon.h"
#ifdef STORM
#include "keys.h"
#else
#include "client/keys.h"
#endif

#include <exec/exec.h>
#include <proto/exec.h>
#include <proto/keymap.h>
#include <intuition/intuition.h>
#include <proto/intuition.h>
#include <graphics/gfx.h>
#include <proto/graphics.h>
#include <sys/stat.h>
#include "dll.h"

cvar_t *nostdout;
cvar_t *serialout = 0;

unsigned int sys_frame_time;
unsigned int sys_msg_time;


uid_t saved_euid;
qboolean stdin_active = true;


// =======================================================================
// General routines
// =======================================================================

void Sys_ConsoleOutput (char *string)
{
	if (serialout && serialout->value)
	    kprintf("%s", string);

	if (nostdout == 0)
		return;
	if (nostdout && nostdout->value)
		return;

	fputs(string, stdout);
}

void Sys_Printf (char *fmt, ...)
{
	va_list         argptr;
	char            text[1024];
	unsigned char           *p;

	va_start (argptr,fmt);
	vsprintf (text,fmt,argptr);
	va_end (argptr);

	if (strlen(text) > sizeof(text))
		Sys_Error("memory overwrite in Sys_Printf");

	if (nostdout == 0)
		return;

	if (nostdout && nostdout->value)
	    return;

	for (p = (unsigned char *)text; *p; p++) {
		*p &= 0x7f;
		if ((*p > 128 || *p < 32) && *p != 10 && *p != 13 && *p != 9)
			printf("[%02x]", *p);
		else
			putc(*p, stdout);
	}
}


struct IntuitionBase *IntuitionBase = 0;
struct GfxBase *GfxBase = 0;
struct Library *SocketBase = 0;
struct Library *KeymapBase = 0;

void Sys_Leave(void)
{
    if (IntuitionBase)      CloseLibrary((struct Library *)IntuitionBase);
    if (GfxBase)            CloseLibrary((struct Library *)GfxBase);
    if (SocketBase)         CloseLibrary(SocketBase);
    if (KeymapBase)         CloseLibrary(KeymapBase);

    IntuitionBase   = NULL;
    GfxBase         = NULL;
    SocketBase      = NULL;
    KeymapBase      = NULL;
}

void Sys_Quit (void)
{
	CL_Shutdown ();
	Qcommon_Shutdown ();
	Sys_Leave();
	exit(0);
}

void Sys_Init(void)
{
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 0L);
    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 0L);
    SocketBase = OpenLibrary("bsdsocket.library", 0L);
    KeymapBase = OpenLibrary("keymap.library", 0);
}

void Sys_Error (char *error, ...)
{ 
    va_list     argptr;
    char        string[1024];

	CL_Shutdown ();
	Qcommon_Shutdown ();
    
    va_start (argptr,error);
    vsprintf (string,error,argptr);
    va_end (argptr);
	fprintf(stderr, "Error: %s\n", string);

	Sys_Leave();
	exit(1);

} 

void Sys_Warn (char *warning, ...)
{ 
    va_list     argptr;
    char        string[1024];
    
    va_start (argptr,warning);
    vsprintf (string,warning,argptr);
    va_end (argptr);
	fprintf(stderr, "Warning: %s", string);
} 

/*
============
Sys_FileTime

returns -1 if not present
============
*/
int     Sys_FileTime (char *path)
{
	struct  stat    buf;
	
	if (stat (path,&buf) == -1)
		return -1;
	
	return buf.st_mtime;
}

char *Sys_ConsoleInput(void)
{
	return NULL;
}

/*****************************************************************************/

static void *game_library;

/*
=================
Sys_UnloadGame
=================
*/
void Sys_UnloadGame (void)
{
	if (game_library) 
		dllFreeLibrary (game_library);
	game_library = NULL;
}

/*
=================
Sys_GetGameAPI

Loads the game dll
=================
*/
void *Sys_GetGameAPI (void *parms)
{
	void    *(*GetGameAPI) (void *);
	void    (*SetExeName) (char *name);

	char    name[MAX_OSPATH];
	char    curpath[MAX_OSPATH];
	char    *path;
#ifdef __PPC__
	const char *gamename = "gameppc.dll";
#else
  const char *gamename = "game68k.dll";
#endif
	void    **SegList;

	if (game_library)
		Com_Error (ERR_FATAL, "Sys_GetGameAPI without Sys_UnloadingGame");

	getcwd(curpath, sizeof(curpath));

	Com_Printf("------- Loading %s -------", gamename);

	// now run through the search paths
	path = NULL;
	while (1)
	{
		path = FS_NextPath (path);
		if (!path)
			return NULL;            // couldn't find one anywhere
		sprintf (name, "%s/%s/%s", curpath, path, gamename);
		game_library = dllLoadLibrary(name, (char *)gamename);
		if (game_library)
		{
			Com_Printf ("\nLoadLibrary (%s)\n",name);
			break;
		}
		else
		{
			Com_Printf("\nLoadLibrary (%s) failed\n", name);
			break;
		}
	}

	SetExeName = (void *)dllGetProcAddress (game_library, "SetExeName");
	if (SetExeName) SetExeName(COM_Argv(0));

	GetGameAPI = (void *)dllGetProcAddress (game_library, "GetGameAPI");
	if (!GetGameAPI)
	{
		Sys_UnloadGame ();              
		return NULL;
	}

	return GetGameAPI (parms);
}

/*****************************************************************************/

void Sys_AppActivate (void)
{
}

#define ENCODEKEY(code,qual) (int)(((int)code & 0x7F) | ((int)qual << 8))


unsigned int mouse_wheel_used = 0; //18-AUG-02

void Sys_SendKeyEvents (void)
{
    struct IntuiMessage *imsg;
    extern struct MsgPort *g_pMessagePort;
    extern struct Window * (*GetWindowHandle)(void);
    extern unsigned int inittime;

    if (GetWindowHandle)
    {
	struct Window *w = GetWindowHandle();
	if (w) g_pMessagePort = w->UserPort;
	if (g_pMessagePort == 0)
	{
	    ModifyIDCMP(w, IDCMP_RAWKEY|IDCMP_MOUSEMOVE|IDCMP_MOUSEBUTTONS|IDCMP_DELTAMOVE);
	    ReportMouse(TRUE, w);
	    g_pMessagePort = w->UserPort;
	}
    }

    while (imsg = (struct IntuiMessage *)GetMsg(g_pMessagePort))
    {
	sys_msg_time=(imsg->Seconds-inittime)*1000+imsg->Micros/1000;
	switch(imsg->Class)
	{
	case IDCMP_RAWKEY:
	    if ((0x7F & imsg->Code) == 122 || (0x7F & imsg->Code) == 123)
	    {
		mouse_wheel_used = 1; //18-AUG-02

		switch(imsg->Code)
		{
		case 0x7A:
		    Key_Event(K_MWHEELUP, true, sys_msg_time, -1); break;
		case 0xFA:
		    Key_Event(K_MWHEELUP, false, sys_msg_time, -1); break;
		case 0x7B:
		    Key_Event(K_MWHEELDOWN, true, sys_msg_time, -1); break;
		case 0xFB:
		    Key_Event(K_MWHEELDOWN, false, sys_msg_time, -1); break;
		}
	    }
	    if ((imsg->Code)&0x80)
		Key_Event(MapKey((imsg->Code)&0x7f),false,sys_msg_time, ENCODEKEY(imsg->Code, imsg->Qualifier));
	    else
		Key_Event(MapKey(imsg->Code),true,sys_msg_time, ENCODEKEY(imsg->Code, imsg->Qualifier));
	    break;
	case IDCMP_MOUSEBUTTONS:
	    switch(imsg->Code)
	    {
	    case IECODE_LBUTTON:
		Key_Event(K_MOUSE1,true,sys_msg_time, -1);
		break;
	    case (IECODE_LBUTTON | IECODE_UP_PREFIX):
		Key_Event(K_MOUSE1,false,sys_msg_time, -1);
		break;
	    case IECODE_RBUTTON:
		Key_Event(K_MOUSE1+1,true,sys_msg_time, -1);
		break;
	    case (IECODE_RBUTTON | IECODE_UP_PREFIX):
		Key_Event(K_MOUSE1+1,false,sys_msg_time, -1);
		break;
	    case IECODE_MBUTTON:
		Key_Event(K_MOUSE1+2,true,sys_msg_time, -1);
		break;
	    case (IECODE_MBUTTON | IECODE_UP_PREFIX):
		Key_Event(K_MOUSE1+2,false,sys_msg_time, -1);
		break;
	    }
	    break;
	case IDCMP_DELTAMOVE:
	case IDCMP_MOUSEMOVE:
	    IN_GetMouseMove(imsg);
	    break;
	}

	ReplyMsg((struct Message *)imsg);
    }

//18-AUG-02: workaround for bug in Topolino3 mouse driver
    if( (!imsg) && (mouse_wheel_used) )
    {
	Key_Event(K_MWHEELUP, false, sys_msg_time, -1);
	Key_Event(K_MWHEELDOWN, false, sys_msg_time, -1);
	mouse_wheel_used = 0;
    }

    // grab frame time
    sys_frame_time = Sys_Milliseconds();
}

/*****************************************************************************/

char *Sys_GetClipboardData(void)
{
	return NULL;
}


//#define MAGAZINTEST

int main (int argc, char **argv)
{
    int     time, oldtime, newtime;

#ifdef MAGAZINTEST
		char id2[100];
    int i;
    int invalid=0;

    CopyMem(0xF00010, id2, 8);
    for (i=0;i<8;i++)
    {
      if (i!=1)
      {
        if (id2[i]!=255) invalid=1;
      }
    }
    if (id2[1]!=65) invalid=1;
    if (invalid) exit(0);
#endif


    Qcommon_Init(argc, argv);

    nostdout = Cvar_Get("nostdout", "1", CVAR_ARCHIVE);
    serialout = Cvar_Get("serialout", "0", 0);

    Com_Printf("\n");
    Com_Printf("  \35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n");
    Com_Printf("\1         Quake \21\20 Amiga\n");
    Com_Printf("  \35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n");
    Com_Printf("           Ported by:\n");
    Com_Printf("       Hans-Joerg Frieden\n");
    Com_Printf("         Thomas Frieden\n");
    Com_Printf("         Steffen Haeuser\n");
    Com_Printf("        Christian Michael\n");
    Com_Printf("  \35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n");
    Com_Printf("\n");

    oldtime = Sys_Milliseconds ();
    while (1)
    {
	do
	{
	    newtime = Sys_Milliseconds ();
	    time = newtime - oldtime;
	} while (time < 1);
	Qcommon_Frame (time);
	oldtime = newtime;
    }

}

void Sys_CopyProtect(void)
{
}

int putenv(const char *buffer)
{
    //FIXME: Implement me
}

void isort(void *pArray, unsigned int nElements, unsigned int nSize, int (*compare)(const void *a, const void *b))
{
    int i, j;
    unsigned char *pCharArray = (unsigned char *)pArray;
    unsigned char *pRef = malloc(nSize);

    for (i = 1; i < nElements; i++)
    {
	memcpy(pRef, pCharArray + nSize * i, nSize);
	j = i;

	while (j > 0 && compare(pCharArray + (j-1)*nSize, pRef) > 0)
	{
	    memcpy(pCharArray + j*nSize, pCharArray + (j-1)*nSize, nSize);
	    j--;
	}

	memcpy(pCharArray + j*nSize, pRef, nSize);
    }

    free(pRef);
}

void Sys_ConvertFilename(char *filename)
{
    while (*filename)
    {
	if (*filename == '\\')
	    *filename = '/';
	filename++;
    }
}

FILE * Sys_fopen(char *filename, char *access)
{
    char name[MAX_OSPATH];

    strcpy(name, filename);

    Sys_ConvertFilename(name);

    return fopen(name, access);
}



int Sys_MapRawKey(int nRawkey)
{
    struct InputEvent ie;
    WORD nResult;
    char cBuffer[100];
    UWORD nCode = nRawkey & 0xFF;
    UWORD nQualifier = (nRawkey >> 8) % 0xFFFF;

    switch (nCode)
    {
	case 65:
	    return -1;

	default:
	    ie.ie_Class         = IECLASS_RAWKEY;
	    ie.ie_SubClass      = 0;
	    ie.ie_Code          = nCode;
	    ie.ie_Qualifier     = nQualifier;
	    ie.ie_EventAddress  = NULL;

	    nResult = MapRawKey(&ie, cBuffer, 100, 0);

	    if (nResult != 1)
		return -1;
	    else
		return cBuffer[0];
    }

    return -1;
}
