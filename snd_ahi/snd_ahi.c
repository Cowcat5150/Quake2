/*
**  snd_ahi.c
**
**  AHI sound driver
**
**  Written by Jarmo Laakkonen <jami.laakkonen@kolumbus.fi>
**  Adopted for Quake 2 Sound DLL by Hans-Joerg Frieden <Hans-JoergF@hyperion-entertainment.com>
**
**  TODO:
**  
**  - Add mode requester
*/

#ifdef __VBCC__
#pragma amiga-align
#elif defined(WARPUP)
#pragma pack(push,2)
#endif

#include <exec/exec.h>
#include <devices/ahi.h>
#include <devices/timer.h>
#include <utility/tagitem.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/timer.h>
#include <clib/alib_protos.h>
#include <proto/ahi.h>

#ifdef __PPC__
#if defined(__GNUC__)
#include <powerpc/powerpc.h>
#include <powerpc/powerpc_protos.h>
#else
#include <powerpc/powerpc.h>
#include <proto/powerpc.h>
#endif
#endif

#ifdef __VBCC__
#pragma default-align
#elif defined(WARPUP)
#pragma pack(pop)
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "../client/client.h"
#include "../client/snd_loc.h"
#include "../amiga/snddll.h"
#include "../amiga/dll.h"

// 68k code in symbol "callback"
//#include "snd_68k.h"

unsigned short callback[] = {
	0x4e55, 0xfff4, 0x2f0a, 0x2b48, 0xfffc, 0x2b4a, 0xfff8, 0x2b49, 0xfff4, 0x206d, 
	0xfffc, 0x226d, 0xfff4, 0x2169, 0x000c, 0x0010, 0x4280, 0x6000, 0x0002, 0x246d, 
	0xfff0, 0x4e5d, 0x4e75, 0x0000
};


#pragma amiga-align
struct ChannelInfo
{
	struct AHIEffChannelInfo cinfo;
	ULONG x[1];
};
#pragma default-align

struct Library *AHIBase = NULL;
static struct MsgPort *AHImp = NULL;
static struct AHIRequest *AHIio = NULL;
static BYTE AHIDevice = -1;
static struct AHIAudioCtrl *actrl = NULL;
static ULONG rc = 1;
static struct ChannelInfo info;

static int speed;
static UBYTE *dmabuf = NULL;
static int buflen;

#define MINBUFFERSIZE 16384

sndimport_t si;

struct Hook EffHook =
{
	0, 0,
	(HOOKFUNC)callback,
	0, 0,
};

DLLFUNC void AHISND_Shutdown(dma_t *dma)
{
	if (actrl)
	{
		info.cinfo.ahie_Effect = AHIET_CHANNELINFO | AHIET_CANCEL;
		AHI_SetEffect(&info, actrl);
		AHI_ControlAudio(actrl, AHIC_Play, FALSE, TAG_END);
	}

	if (rc == 0)
	{
		AHI_UnloadSound(0, actrl);
		rc = 1;
	}

	if (dmabuf)
	{
		#ifdef __PPC__	
		FreeVecPPC(dmabuf);
		#else
		FreeVec(dmabuf);
		#endif
  
		dmabuf = NULL;
	}

	if (actrl)
	{
		AHI_FreeAudio(actrl);
		actrl = NULL;
	}

	if (AHIDevice == 0)
	{
		CloseDevice((struct IORequest *)AHIio);
		AHIDevice = -1;
	}

	if (AHIio)
	{
		DeleteIORequest((struct IORequest *)AHIio);
		AHIio = NULL;
	}

	if (AHImp)
	{
		DeleteMsgPort(AHImp);
		AHImp = NULL;
	}
}

DLLFUNC qboolean AHISND_Init(dma_t *dma)
{
	struct AHISampleInfo sample;
	int			i;
	ULONG			mixfreq, playsamples;
	UBYTE			name[256];
	ULONG			mode;
	ULONG			type;
	int			ahichannels;
	int			ahibits;
	cvar_t			*cv;

	info.cinfo.ahieci_Channels = 1;
	info.cinfo.ahieci_Func = &EffHook;
	info.cinfo.ahie_Effect = AHIET_CHANNELINFO;
	EffHook.h_Data = 0;

	cv = si.Cvar_Get("ahi_speed", "22", CVAR_ARCHIVE);

	if (cv->value == 22)
		speed = 22050;

	else if (cv->value == 11)
		speed = 11025;

	else
		speed = 11025;

	cv = si.Cvar_Get("ahi_channels", "2", CVAR_ARCHIVE);

	if (cv->value == 2)
		ahichannels = 2;

	else if (cv->value == 1)
		ahichannels = 1;

	else ahichannels = 2;

	cv = si.Cvar_Get("ahi_bits", "16", CVAR_ARCHIVE);

	if (cv->value == 16)
		ahibits = 16;

	else if (cv->value == 8)
		ahibits = 8;

	else ahibits = 16;

	if (ahibits != 8 && ahibits != 16)
		ahibits = 16;

	if (ahichannels == 1)
	{
		if (ahibits == 16)
			type = AHIST_M16S;

		else
			type = AHIST_M8S;
	}

	else
	{
		if (ahibits == 16)
			type = AHIST_S16S;

		else
			type = AHIST_S8S;
	}

	if ((AHImp = CreateMsgPort()) == NULL)
	{
		si.Con_Printf(PRINT_ALL, "ERROR: Can't create AHI message port\n");
		return false;
	}

	if ((AHIio = (struct AHIRequest *)CreateIORequest(AHImp, sizeof(struct AHIRequest))) == NULL)
	{
		si.Con_Printf(PRINT_ALL, "ERROR:Can't create AHI io request\n");
		return false;
	}

	AHIio->ahir_Version = 4;

	if ((AHIDevice = OpenDevice("ahi.device", AHI_NO_UNIT, (struct IORequest *)AHIio, 0)) != 0)
	{
		si.Con_Printf(PRINT_ALL, "Can't open ahi.device version 4\n");
		return false;
	}

	AHIBase = (struct Library *)AHIio->ahir_Std.io_Device;

	if ((actrl = AHI_AllocAudio(AHIA_AudioID, AHI_DEFAULT_ID,
			AHIA_MixFreq, speed,
			AHIA_Channels, 1,
			AHIA_Sounds, 1,
			TAG_END)) == NULL)
	{
		si.Con_Printf(PRINT_ALL, "Can't allocate audio\n");
		return false;
	}

	AHI_GetAudioAttrs(AHI_INVALID_ID, actrl, AHIDB_MaxPlaySamples, (ULONG)&playsamples,
		AHIDB_BufferLen, 256, AHIDB_Name, (ULONG)&name,
		AHIDB_AudioID, (ULONG)&mode, TAG_END);

	AHI_ControlAudio(actrl, AHIC_MixFreq_Query, (ULONG)&mixfreq, TAG_END);
	buflen = playsamples * speed / mixfreq;

	if (buflen < MINBUFFERSIZE)
		buflen = MINBUFFERSIZE;

	#ifdef __PPC__
	if ((dmabuf = AllocVecPPC(buflen, MEMF_ANY | MEMF_PUBLIC | MEMF_CLEAR, 0)) == NULL) {
	#else
	if ((dmabuf = AllocVec(buflen, MEMF_ANY|MEMF_PUBLIC|MEMF_CLEAR)) == NULL) {
	#endif
		si.Con_Printf(PRINT_ALL, "Can't allocate AHI dma buffer\n");
		return false;
	}

	dma->buffer = (unsigned char *)dmabuf;
	dma->channels = ahichannels;
	dma->speed = speed;
	dma->samplebits = ahibits;
	dma->samples = buflen / (dma->samplebits / 8);
	dma->submission_chunk = 1;
	dma->samplepos = 0;

	sample.ahisi_Type = type;
	sample.ahisi_Address = (APTR)dmabuf;
	sample.ahisi_Length = buflen / AHI_SampleFrameSize(type);

	if ((rc = AHI_LoadSound(0, AHIST_DYNAMICSAMPLE, &sample, actrl)) != 0)
	{
		si.Con_Printf(PRINT_ALL, "Can't load sound\n");
		return false;
	}
 
	if (AHI_ControlAudio(actrl, AHIC_Play, TRUE, TAG_END) != 0)
	{
		si.Con_Printf(PRINT_ALL, "Can't start playback\n");
		return false;
	}

	si.Con_Printf(PRINT_ALL, "AHI audio initialized\n");
	si.Con_Printf(PRINT_ALL, "AHI mode: %s (%08x)\n", name, mode);
	si.Con_Printf(PRINT_ALL, "Output: %ibit %s\n", ahibits, ahichannels == 2 ? "stereo" : "mono");
	si.Con_Printf(PRINT_ALL, "snd_ahi.dll\nwritten by Jarmo Laakkonen and Hans-Joerg Frieden\n");

	AHI_Play(actrl, AHIP_BeginChannel, 0,
		AHIP_Freq, speed,
		AHIP_Vol, 0x10000,
		AHIP_Pan, 0x8000,
		AHIP_Sound, 0,
		AHIP_EndChannel, NULL,
		TAG_END);

	AHI_SetEffect(&info, actrl);
	return true;
}

DLLFUNC int AHISND_GetDMAPos(dma_t *dma)
{
	return (dma->samplepos = (int)(EffHook.h_Data) * dma->channels);
}

/* ========== EXPORTED FUNCTIONS ================================================ */

DLLFUNC sndexport_t GetSndAPI(sndimport_t simp)
{
	sndexport_t se;

	si = simp;

	se.api_version	    = SND_API_VERSION;
	se.Init		    = AHISND_Init;
	se.Shutdown	    = AHISND_Shutdown;
	se.GetDMAPos	    = AHISND_GetDMAPos;
	se.BeginPainting    = NULL;
	se.Submit	    = NULL;

	return se;
}

#if 0
DLLFUNC void*  dllFindResource(int id, char *pType)
{
	return NULL;
}


DLLFUNC void* dllLoadResource(void *pHandle)
{
	return NULL;
}

DLLFUNC void dllFreeResource(void *pHandle)
{
	return;
}
#endif

dll_tExportSymbol DLL_ExportSymbols[] =
{
	//{(void *)dllFindResource,"dllFindResource"},
	//{(void *)dllLoadResource,"dllLoadResource"},
	//{(void *)dllFreeResource,"dllFreeResource"},
	{(void *)GetSndAPI,"GetSndAPI"},
	{0,0},
};

dll_tImportSymbol DLL_ImportSymbols[] =
{
	{0,0,0,0}
};

int DLL_Init(void)
{
	return 1;
}

void DLL_DeInit(void)
{
}

#if defined (__GNUC__)
extern int main(int, char **); // new Cowcat
#endif

