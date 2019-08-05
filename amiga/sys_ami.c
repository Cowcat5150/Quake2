// sys_null.h -- null system driver to aid porting efforts

#include "../qcommon/qcommon.h"
#include "errno.h"
#include "keys.h"

#include <clib/chunkyppc_protos.h>

#include <proto/exec.h>

#include <proto/dos.h>

#include <proto/keymap.h>

unsigned int timeGetTime(void);

#define MAX_NUM_ARGVS 128
int           argc;
char *            argv[MAX_NUM_ARGVS+1];

#define AMIGA_CD_NAME "QUAKE2"

int	curtime;

unsigned	sys_frame_time;
unsigned int sys_msg_time;

struct Library *KeymapBase;

char *Sys_ScanForCD (void)
{
  FILE *fil=fopen("env:Quake2/cdpath","r");
  char *p;
  if (!fil) return 0;
  p=getenv("Quake2/cdpath");
  if (fil) fclose(fil);
  return p;
}


void Sys_Error (char *error, ...)
{
	va_list		argptr;

	printf ("Sys_Error: ");	
	va_start (argptr,error);
	vprintf (error,argptr);
	va_end (argptr);
	printf ("\n");

	exit (1);
}

void Sys_Quit (void)
{
	exit (0);
}

static void * game_library = NULL;

void	Sys_UnloadGame (void)
{
  if(game_library) dllFreeLibrary(game_library);
  game_library = NULL;
}

void	*Sys_GetGameAPI (void *parms)
{
  char    *path;
  void    *h=NULL;
  char    libname[MAX_OSPATH];
	char name[1024];
	char curpath[1024];

	void *(*GetGameAPI) (void *);

  strcpy(name,"game.dll");

	if (game_library) Com_Error (ERR_FATAL, "Sys_GetGameAPI without Sys_UnloadingGame");

	Com_Printf("------- Loading %s -------", "game.dll");

  // now run through the search paths
  path = NULL;

  while (1)
	{
 		path = FS_NextPath (path);
		if (!path) return NULL;
 		sprintf (name, "%s/%s", path, "game.dll");

		strcpy(libname,"game.dll");

    game_library = dllLoadLibrary (libname, (char *)name);
    if (game_library)
		{
 			Com_DPrintf ("LoadLibrary (%s)\n",name);
 			break;
		}
	}

  if(!game_library)
  {
      Sys_Error("Failed to load %s\n", name);
  }

  Com_DPrintf ("LoadLibrary (%s)\n", libname);
	
  GetGameAPI = dllGetProcAddress(game_library,"GetGameAPI");
	if (!GetGameAPI)
  {
		Sys_UnloadGame ();
 		return NULL;
	}
	return GetGameAPI (parms);
}

static char   console_text[256];
static int    console_textlen=0;

BPTR serverinput=0L;

void Sys_CloseInput(void)
{
  Close(serverinput);
}

char *Sys_ConsoleInput (void)
{
  char ch;
  static char s[100];
  static flag=0;

  if (!dedicated || !dedicated->value)
      return NULL;

  if(!serverinput)
  {
      serverinput=Open("CON:0/0/500/100/SERVER_INPUT/AUTO/CLOSE", MODE_OLDFILE);
      atexit(Sys_CloseInput);
  }

  while (1) {
      if (WaitForChar(serverinput,0))     /* there is at least one key to get */
      {
	  if (Read(serverinput,&ch,1)!=1)
	  {
	      fprintf(stderr,"read failed, huh? (%s)\n",strerror(errno));
	      return NULL;
	  }
	  switch(ch)
	  {
	      case '\r':
	      case '\n':
		  if (console_textlen)
		  {
		      console_text[console_textlen] = 0;
		      console_textlen = 0;
		      return console_text;
		  }
		  break;
	      case '\b':
		  if (console_textlen)
		  {
		      console_textlen--;
		  }
		  break;
	      default:
		  if (ch >= ' ')
		  {
		      if (console_textlen < (signed)sizeof(console_text)-3)
		      {
			  /* write(1, &ch, 1); - ECHO */
			  console_text[console_textlen++] = ch;
		      }
		  }
	  }
      }
      else
	  return NULL;    /* no more key events */
  }
  return NULL;
}

void	Sys_ConsoleOutput (char *string)
{
  char    text[256];

  if (!dedicated || !dedicated->value)
      return;

  if (console_textlen)
  {
      text[0] = '\r';
      memset(text+1, ' ', console_textlen);
      text[console_textlen+1] = '\r';
      text[console_textlen+2] = 0;
      write(1,text,console_textlen+2);
  }

  write(1,string,strlen(string));

  if (console_textlen)
      write(1,console_text,console_textlen);
}

extern struct Mode_Screen * (*GetModeScreen)();

#define ENCODEKEY(code,qual) (int)(((int)code & 0x7F) | ((int)qual << 8))

extern unsigned int inittime;

void Sys_SendKeyEvents(void)
{
  struct Mode_Screen *ms;
  struct IntuiMessage *imsg;

  if(!GetModeScreen)
    return;

  ms=GetModeScreen();

  do
  {
    extern void IN_GetMouseMove(struct IntuiMessage *);

    imsg=(struct IntuiMessage *)GetMsg((ms->video_window)->UserPort);
    if (imsg)
    {
	sys_msg_time=(imsg->Seconds-inittime)*1000+imsg->Micros/1000;
	switch(imsg->Class)
	{
	    case IDCMP_RAWKEY:
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
	case IDCMP_MOUSEMOVE:
	      IN_GetMouseMove(imsg);
	      break;
	}
	if (ms->video_window) ReplyMsg(imsg);
    }
  } while(imsg);
  sys_frame_time=timeGetTime();
}

void Sys_AppActivate (void)
{
}

char *Sys_GetClipboardData( void )
{
	return NULL;
}

static void CloseKeymap(void)
{
    if (KeymapBase) CloseLibrary(KeymapBase);
    KeymapBase = 0;
}

void	Sys_Init (void)
{
  KeymapBase = OpenLibrary("keymap.library", 0);
  atexit(CloseKeymap);
}


//=============================================================================

int amigaRawKeyConvert(int rawkey)
{
    struct InputEvent ie;
    WORD res;
    char cbuf[20];

    UWORD code = rawkey&0xFF;
    UWORD qual = (rawkey>>8)%0xFFFF;
    if (!KeymapBase) return -1;

    switch(code)
    {
    case 61:
    case 62:
    case 63:
    case 45:
    case 46:
    case 47:
    case 29:
    case 30:
    case 31:
    case 15:
    case 60:
	return -1;
    default:
	ie.ie_Class = IECLASS_RAWKEY;
	ie.ie_SubClass = 0;
	ie.ie_Code  = code;
	ie.ie_Qualifier = qual;
	ie.ie_EventAddress = NULL;

	res = MapRawKey(&ie, cbuf, 20, 0);
	if (res != 1)
	{
	    return -1;
	}
	else
	{
	    return cbuf[0];
	}
    }
}


int main (int argc_, char **argv_)
{
 	char *cddir;
  int time, oldtime, newtime;

	argc=(argc_>MAX_NUM_ARGVS)?MAX_NUM_ARGVS:argc_;
  memcpy(argv,argv_,sizeof *argv*argc);   /* keep our own [guaranteed mutable] copy */
	argv[argc]=NULL;

#ifdef AMIGA
  cddir = AMIGA_CD_NAME ":";
#else
  cddir = Sys_ScanForCD ();
#endif
  if (cddir && argc < MAX_NUM_ARGVS - 3)
  {
      int     i;

      // don't override a cddir on the command line
      for (i=0 ; i<argc ; i++)
	  if (!Q_strcasecmp(argv[i], "-cddir"))
	      break;
      if (i == argc)
      {
	  argv[argc++] = "-cddir";
	  argv[argc++] = cddir;
      }
  }

	Qcommon_Init (argc, argv);
  oldtime = Sys_Milliseconds ();

	while (1)
	{
    if (dedicated && dedicated->value)
    {
	   Sleep (1);
    }

    do
    {
	  	newtime = Sys_Milliseconds ();
	  	time = newtime - oldtime;
    } while (time < 1);

		Qcommon_Frame (time);
    oldtime=newtime;
	}
  return 1;
}

void Sleep(unsigned mili)
{
}
