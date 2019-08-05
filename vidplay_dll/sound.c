#include "/client/client.h"
#include "/amiga/dll.h"

typedef struct
{
 int   	channels;
 int   	samples;    // mono samples in buffer
 int   	submission_chunk;  // don't mix less than this #
 int   	samplepos;    // in mono samples
 int   	samplebits;
 int   	speed;
 unsigned char  *buffer;
} dma_t;

#define PAINTBUFFER_SIZE 2048
#define MAX_RAW_SAMPLES 8192

typedef struct
{
 int   left;
 int   right;
} portable_samplepair_t;

dma_t 	dma;
int 	paintedtime;
int 	*snd_p, snd_linear_count, snd_vol;
short 	*snd_out;
portable_samplepair_t   s_rawsamples[MAX_RAW_SAMPLES];
portable_samplepair_t   paintbuffer[PAINTBUFFER_SIZE];

void SNDDMA_Shutdown(void);

void SNDDMA_BeginPainting (void);

void SNDDMA_Submit(void);

#define SND_API_VERSION 1

typedef struct
{
    void    (*Sys_Error) (int err_level, char *str, ...);

    void    (*Cmd_AddCommand) (char *name, void(*cmd)(void));
    void    (*Cmd_RemoveCommand) (char *name);
    int     (*Cmd_Argc) (void);
    char    *(*Cmd_Argv) (int i);
    void    (*Cmd_ExecuteText) (int exec_when, char *text);

    void    (*Con_Printf) (int print_level, char *str, ...);

    int     (*FS_LoadFile) (char *name, void **buf);
    void    (*FS_FreeFile) (void *buf);

    char    *(*FS_Gamedir) (void);

    cvar_t  *(*Cvar_Get) (char *name, char *value, int flags);
    cvar_t  *(*Cvar_Set)( char *name, char *value );
    void     (*Cvar_SetValue)( char *name, float value );
} sndimport_t;

typedef struct
{
    int 	api_version;

    int		(*Init)(dma_t *dma);
    void	(*Shutdown)(dma_t *dma);

    int  	(*GetDMAPos)(dma_t *dma);
    void	(*BeginPainting)(dma_t *dma);
    void	(*Submit)(dma_t *dma);
} sndexport_t;

typedef sndexport_t     (*GetSndAPI_t) (sndimport_t);

int 	use_paula=1;
float 	s_volume=0.7,s_mixahead;
int 	my_s_khz,s_mixsound,s_testsound,s_show,s_primary;
long 	soundtime;
int 	s_rawend=0;

void 	*sound_library;
sndexport_t se;


int     SNDDMA_GetDMAPos(void);
int     SNDDMA_Init(void);


void GetSoundtime(void)
{
 int             samplepos;
 static  int     buffers;
 static  int     oldsamplepos;
 int             fullsamples;

 fullsamples = dma.samples / dma.channels;

 samplepos = SNDDMA_GetDMAPos();

 if (samplepos < oldsamplepos)
 {
  buffers++;                                      // buffer wrapped

  if (paintedtime > 0x40000000)
  {       // time to chop things off to avoid 32 bit limits
   buffers = 0;
   paintedtime = fullsamples;
  }
 }
 oldsamplepos = samplepos;

 soundtime = buffers*fullsamples + samplepos/dma.channels;
}

void S_ClearBuffer (void)
{
 int             clear;

 s_rawend = 0;

 clear = 0;

 SNDDMA_BeginPainting ();
 if (dma.buffer) memset(dma.buffer, clear, dma.samples * dma.samplebits/8);
 SNDDMA_Submit ();
}

void VID_Printf (int print_level, char *fmt, ...)
{
}

void VID_Error (int err_level, char *fmt, ...)
{
}

cvar_t ahi_speed={"ahi_speed","22","22",0,0,22,0};
cvar_t ahi_channels={"ahi_channels","2","2",0,0,2,0};
cvar_t ahi_bits={"ahi_bits","8","8",0,0,16,0};
cvar_t s_khz={"s_khz","22","22",0,0,22,0};
cvar_t s_default={"default","0","0",0,0,0,0};

cvar_t *Cvar_Get (char *var_name, char *var_value, int flags)
{
  if (!strcmp(var_name,"ahi_speed")) return &ahi_speed;
  else if (!strcmp(var_name,"ahi_channels")) return &ahi_channels;
  else if (!strcmp(var_name,"ahi_bits")) return &ahi_bits;
  else if (!strcmp(var_name,"s_khz")) return &s_khz;
  else return &s_default;
}

void SND_FreeSoundlib(void)
{
    if (sound_library) dllFreeLibrary(sound_library);
    memset (&se, 0, sizeof(se));
}

void dummy_function()
{
}

int SND_LoadSoundlib(char *name)
{
    sndimport_t     si;
    GetSndAPI_t     GetSndAPI;

    if ( (sound_library = dllLoadLibrary(name, name)) == 0)
    {
 		return 0;
    }

    si.Cmd_AddCommand = dummy_function;
    si.Cmd_RemoveCommand = dummy_function;
    si.Cmd_Argc = dummy_function;
    si.Cmd_Argv = dummy_function;
    si.Cmd_ExecuteText = dummy_function;
    si.Con_Printf = dummy_function; //VID_Printf;
    si.Sys_Error = dummy_function; //VID_Error;
    si.FS_LoadFile = dummy_function;
    si.FS_FreeFile = dummy_function;
    si.FS_Gamedir = dummy_function;
    si.Cvar_Get = dummy_function; //Cvar_Get;
    si.Cvar_Set = dummy_function;
    si.Cvar_SetValue = dummy_function;

    if ( ( GetSndAPI = (void *) dllGetProcAddress( sound_library, "GetSndAPI" ) ) == 0 )		
      {}

    se = GetSndAPI( si );

    if (se.api_version != SND_API_VERSION)
    {
 			SND_FreeSoundlib ();
 			//printf(ERR_FATAL, "%s has incompatible snd_api_version", name);
    }

    if ( se.Init( &dma ) == -1 )
    {
 			se.Shutdown(&dma);
 			SND_FreeSoundlib ();
 			return false;
    }

		printf("Sound correctly initialized.\n");

    return 1;
}

void S_WriteLinearBlastStereo16 (void)
{
 int             i;
 int             val;

 for (i=0 ; i<snd_linear_count ; i+=2)
 {
  val = snd_p[i]>>8;
  if (val > 0x7fff)
   snd_out[i] = 0x7fff;
  else if (val < (short)0x8000)
   snd_out[i] = (short)0x8000;
  else
   snd_out[i] = val;

  val = snd_p[i+1]>>8;
  if (val > 0x7fff)
   snd_out[i+1] = 0x7fff;
  else if (val < (short)0x8000)
   snd_out[i+1] = (short)0x8000;
  else
   snd_out[i+1] = val;
 }
}

void S_TransferStereo16 (unsigned long *pbuf, int endtime)
{
 int             lpos;
 int             lpaintedtime;

 snd_p = (int *) paintbuffer;
 lpaintedtime = paintedtime;

 while (lpaintedtime < endtime)
 {
  lpos = lpaintedtime & ((dma.samples>>1)-1);

  snd_out = (short *) pbuf + (lpos<<1);

  snd_linear_count = (dma.samples>>1) - lpos;
  if (lpaintedtime + snd_linear_count > endtime) snd_linear_count = endtime - lpaintedtime;

  snd_linear_count <<= 1;

  S_WriteLinearBlastStereo16 ();

  snd_p += snd_linear_count;
  lpaintedtime += (snd_linear_count>>1);
 }
}

void S_TransferPaintBuffer(int endtime)
{
	int     out_idx;
	int     count;
	int     out_mask;
	int     *p;
	int     step;
	int             val;
	unsigned long *pbuf;

	pbuf = (unsigned long *)dma.buffer;

	if (dma.samplebits == 16 && dma.channels == 2)
	{
		S_TransferStereo16 (pbuf, endtime);
	}
	else
	{
		p = (int *) paintbuffer;
		count = (endtime - paintedtime) * dma.channels;
		out_mask = dma.samples - 1;
		out_idx = paintedtime * dma.channels & out_mask;
		step = 3 - dma.channels;

		if (dma.samplebits == 16)
		{
			short *out = (short *) pbuf;
			while (count--)
			{
				val = *p >> 8;
				p+= step;
				if (val > 0x7fff)
					val = 0x7fff;
				else if (val < (short)0x8000)
					val = (short)0x8000;
				out[out_idx] = val;
				out_idx = (out_idx + 1) & out_mask;
			}
		}
		else if (dma.samplebits == 8)
		{
			char *out = (char *) pbuf;
			while (count--)
			{
				val = *p >> 8;
				p+= step;
				if (val > 0x7fff)
					val = 0x7fff;
				else if (val < (short)0x8000)
					val = (short)0x8000;
				out[out_idx] = (val/256);
				out_idx = (out_idx + 1) & out_mask;
			}
		}
	}
}

void S_PaintChannels(int endtime)
{
	int     i;
	int     end;
	int             ltime, count;
	float snd_vol;

	snd_vol = s_volume*256;

	while (paintedtime < endtime)
	{
		end = endtime;
		if (endtime - paintedtime > PAINTBUFFER_SIZE) end = paintedtime + PAINTBUFFER_SIZE;

		if (s_rawend < paintedtime)
		{
			memset(paintbuffer, 0, (end - paintedtime) * sizeof(portable_samplepair_t));
		}
		else
		{
			int             s;
			int             stop;

			stop = (end < s_rawend) ? end : s_rawend;

			for (i=paintedtime ; i<stop ; i++)
			{
				s = i&(MAX_RAW_SAMPLES-1);
				paintbuffer[i-paintedtime] = s_rawsamples[s];
			}
			for ( ; i<end ; i++)
			{
				paintbuffer[i-paintedtime].left =
				paintbuffer[i-paintedtime].right = 0;
			}
		}

		S_TransferPaintBuffer(end);
		paintedtime = end;
	}
}
           
void S_Update_(void)
{
 unsigned        endtime;
 int             samps;

 SNDDMA_BeginPainting ();

 if (!dma.buffer) return;

 GetSoundtime();

 if (paintedtime < soundtime)
 {
  paintedtime = soundtime;
 }

 endtime = soundtime + s_mixahead * dma.speed;
 endtime = (endtime + dma.submission_chunk-1) & ~(dma.submission_chunk-1);
 samps = dma.samples >> (dma.channels-1);
 if (endtime - soundtime > samps) endtime = soundtime + samps;

 S_PaintChannels (endtime);

 SNDDMA_Submit ();
}

void S_Init (void)
{

  my_s_khz = 11;
  s_mixahead = 0.2;
  s_show = 0;
  s_testsound = 0;
  s_primary = 0;

  if (!SNDDMA_Init()) return;

  soundtime = 0;
  paintedtime = 0;

  S_ClearBuffer();
}

void S_Shutdown(void)
{
 SNDDMA_Shutdown();
}

int SNDDMA_Init(void)
{
    if (use_paula) return SND_LoadSoundlib("snd_paula.dll");
    else return SND_LoadSoundlib("snd_ahi.dll");
}

int  SNDDMA_GetDMAPos(void)
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
    if (se.BeginPainting) se.BeginPainting(&dma);
}

void SNDDMA_Submit(void)
{
    if (se.Submit) se.Submit(&dma);
}
