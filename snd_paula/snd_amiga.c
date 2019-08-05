
// snddma_null.c
// all other sound mixing is portable

#include "../client/client.h"
#include "../client/snd_loc.h"
#include "../amiga/snddll.h"

#ifdef __VBCC__
#pragma amiga-align
#elif defined(WARPUP)
#pragma pack(push,2)
#endif

//#include <clib/timer_protos.h> // Cowcat
#include <exec/exec.h>
#include <exec/interrupts.h>
#include <devices/audio.h>
//#include <devices/timer.h> // Cowcat
#include <hardware/custom.h>
#include <hardware/intbits.h>
#include <hardware/dmabits.h>
#include <proto/exec.h>
#include <proto/timer.h>

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

//#include "snd_interrupt.h"

unsigned short intcode[] = {
	0x48e7, 0xc0c2, 0x2c69, 0x0008, 0x2049, 0xd1fc, 0x0000, 0x0010, 0x4eae, 0xffbe, 
	0x4cdf, 0x4303, 0x2029, 0x000c, 0x3140, 0x009c, 0x7000, 0x4e75
};

#include "../amiga/dll.h"

static struct Interrupt AudioInt = {{NULL, NULL, NT_INTERRUPT, 100, "Quake2 Sound Interrupt"}, NULL, NULL};
static struct Custom *	custom = (struct Custom *)0xdff000;
static struct Interrupt *oldInt = 0;
//static struct timeval starttime;
struct Library *TimerBase = 0;	// Now struct Library Cowcat
WORD *g_pLeft = 0;
WORD *g_pRight = 0;

sndimport_t si;

#if 0 // old gcc Cowcat

#ifdef __PPC__
#define BeginIO(ioRequest) _BeginIO(ioRequest)

inline void _BeginIO(struct IORequest *ioRequest)
{
	struct PPCArgs args;
	memset(&args,0,sizeof(args));
	args.PP_Code		= (APTR)ioRequest->io_Device;
	args.PP_Offset		= (-30);
	args.PP_Flags		= 0;
	args.PP_Regs[PPREG_A1]	= (ULONG)ioRequest;
	args.PP_Regs[PPREG_A6]	= (ULONG)ioRequest->io_Device;
	Run68K (&args);
}

#endif
#endif

static struct MsgPort *g_pAudioMP = 0;
static struct IOAudio *g_pAudioIO = 0;
static struct timerequest *g_pTimeIO = 0;

typedef struct
{
	BYTE	*m_pBufferLeft		;
	BYTE	*m_pBufferRight		;
	struct	Library *m_pTimerBase	;
	ULONG	m_nIntMask		;
	struct	timeval m_tv		;

} intdata_t;

int snd_speed = 22050;
int snd_size = 0x4000;


DLLFUNC qboolean PAULA_Init(dma_t *dma)
{
	intdata_t	*pInt;
	UBYTE		alloc_mask[] = {3};
	UWORD		period;
	cvar_t		*s_khz = si.Cvar_Get("s_khz", "22", CVAR_ARCHIVE);

	si.Con_Printf(PRINT_DEVELOPER, "Initializing DMA sound\n");

	if (s_khz->value == 22)
		snd_speed = 22050;

	else
		snd_speed = 11025;

	period = (UWORD)(1.0 / (snd_speed * 2.79365e-7));

	g_pAudioMP = CreateMsgPort();

	if (g_pAudioMP)
	{
		#ifdef __PPC__	  
		g_pAudioIO = (struct IOAudio *)AllocVecPPC(sizeof(struct IOAudio), MEMF_PUBLIC|MEMF_CLEAR, 0);
		#else
		g_pAudioIO = (struct IOAudio *)AllocVec(sizeof(struct IOAudio), MEMF_PUBLIC|MEMF_CLEAR);
		#endif

		if (g_pAudioIO)
		{
			g_pAudioIO->ioa_Request.io_Message.mn_ReplyPort	    = g_pAudioMP;
			g_pAudioIO->ioa_Request.io_Message.mn_Node.ln_Pri   = 127;
			g_pAudioIO->ioa_Request.io_Command		    = ADCMD_ALLOCATE;
			g_pAudioIO->ioa_Request.io_Flags		    = ADIOF_NOWAIT;
			g_pAudioIO->ioa_AllocKey			    = 0;
			g_pAudioIO->ioa_Data				    = alloc_mask;
			g_pAudioIO->ioa_Length				    = sizeof(alloc_mask);

			if (OpenDevice(AUDIONAME, 0, (struct IORequest *)g_pAudioIO, 0) != 0)
			{
				si.Con_Printf(PRINT_ALL, "Error: Can't allocate audio channels\n");

				#ifdef __PPC__
				FreeVecPPC(g_pAudioIO);
				#else
				FreeVec(g_pAudioIO);
				#endif

				g_pAudioIO  = 0;
				DeleteMsgPort(g_pAudioMP);
				g_pAudioMP  = 0;
				return false;
			}
		}

		else
		{
			si.Con_Printf(PRINT_ALL, "Error: Can't allocate memory for IO request\n");
			DeleteMsgPort(g_pAudioMP);
			g_pAudioMP  = 0;
			return false;
		}
	}

	else
	{
		si.Con_Printf(PRINT_ALL, "ERROR:Can't create message port for audio IO\n");
		return false;
	}

	#ifdef __PPC__
	g_pTimeIO = AllocVecPPC(sizeof(struct timerequest), MEMF_CLEAR|MEMF_PUBLIC, 0);
	#else
	g_pTimeIO = AllocVec(sizeof(struct timerequest), MEMF_CLEAR|MEMF_PUBLIC);
	#endif

	if (g_pTimeIO && OpenDevice(TIMERNAME, UNIT_MICROHZ, (struct IORequest*)g_pTimeIO, 0) == 0)
	{
		TimerBase = (struct Library *)g_pTimeIO->tr_node.io_Device;
	}

	else
	{
		si.Con_Printf(PRINT_ALL, "Error: Can't open timer\n");
		return false;
	}

	dma->channels		= 1;
	dma->samples		= snd_size;
	dma->submission_chunk	= 1;
	dma->samplepos		= 0;
	dma->samplebits		= 8;
	dma->speed		= snd_speed;

	#ifdef __PPC__	  
	dma->buffer		= AllocVecPPC(dma->samples*dma->channels, MEMF_CHIP|MEMF_CLEAR, 0);
	AudioInt.is_Data	= AllocVecPPC(sizeof(intdata_t), MEMF_CHIP|MEMF_CLEAR|MEMF_PUBLIC, 0);

	#else

	dma->buffer		= AllocVec(dma->samples*dma->channels, MEMF_CHIP|MEMF_CLEAR);
	AudioInt.is_Data	= AllocVec(sizeof(intdata_t), MEMF_CHIP|MEMF_CLEAR|MEMF_PUBLIC);
	#endif

	AudioInt.is_Code = (void (*))intcode;

	pInt = (intdata_t *)(AudioInt.is_Data);
	//    pInt->m_pBufferLeft = g_pLeft   = AllocVecPPC(snd_size, MEMF_CHIP|MEMF_CLEAR, 0);
	//    pInt->m_pBufferRight= g_pRight  = AllocVecPPC(snd_size, MEMF_CHIP|MEMF_CLEAR, 0);
	pInt->m_pTimerBase  = TimerBase;
	pInt->m_nIntMask    = INTF_AUD0;

	custom->dmacon = DMAF_AUD0;
	custom->intena = INTF_AUD0;
	custom->intreq = INTF_AUD0;

	oldInt = SetIntVector(INTB_AUD0, &AudioInt);

	custom->aud[0].ac_len = snd_size>>1;
	custom->aud[0].ac_per = period;
	custom->aud[0].ac_vol = 64;
	custom->aud[1].ac_len = snd_size>>1;
	custom->aud[1].ac_per = period;
	custom->aud[1].ac_vol = 64;

	custom->aud[0].ac_ptr = (UWORD *)dma->buffer; //(pInt->m_pBufferLeft);
	custom->aud[1].ac_ptr = (UWORD *)dma->buffer; //(pInt->m_pBufferRight);

	custom->intena = INTF_SETCLR|INTF_INTEN|INTF_AUD0;
	custom->dmacon = DMAF_SETCLR|DMAF_AUD0|DMAF_AUD1;

	si.Con_Printf(PRINT_ALL, "snd_paula.dll\nwritten by Thomas and Hans-Joerg Frieden\n");

	return true;
}

DLLFUNC int PAULA_GetDMAPos(dma_t *dma)
{
	struct timeval endtime;
	intdata_t *pInt = (intdata_t *)(AudioInt.is_Data);

	if (!pInt)
		return 0;

	GetSysTime(&endtime);

	#ifdef __PPC__	  
	SubTimePPC(&endtime, &(pInt->m_tv));
	#else
	SubTime(&endtime, &(pInt->m_tv));
	#endif

	dma->samplepos = (int)((double)endtime.tv_micro*(double)snd_speed/1000000.0);

	if (dma->samplepos > snd_size)
		dma->samplepos = 0;

	return dma->samplepos;
}

DLLFUNC void PAULA_Shutdown(dma_t *dma)
{
	//intdata_t *pInt;

	if (oldInt)
	{
		//pInt = (intdata_t *)(AudioInt.is_Data);

		custom->intena = INTF_AUD0;
		SetIntVector(INTB_AUD0, oldInt);

		#ifdef __PPC__
		FreeVecPPC(AudioInt.is_Data);
		#else
		FreeVec(AudioInt.is_Data);
		#endif	

		oldInt = 0;
	}

	if (TimerBase)
	{
		CloseDevice((struct IORequest *)g_pTimeIO);

		#ifdef __PPC__
		FreeVecPPC(g_pTimeIO);
		#else
		FreeVec(g_pTimeIO);
		#endif

		g_pTimeIO = 0;
		TimerBase = 0;
	}

	if (!g_pAudioMP || ! g_pAudioIO)
		return;

	if (CheckIO((struct IORequest *)g_pAudioIO))
	{
		AbortIO((struct IORequest *)g_pAudioIO);
		WaitPort(g_pAudioMP);
	}

	CloseDevice((struct IORequest *)g_pAudioIO);

#ifdef __PPC__
	FreeVecPPC(g_pAudioIO);
#else
	FreeVec(g_pAudioIO);
#endif

	g_pAudioIO = 0;
	DeleteMsgPort(g_pAudioMP);
	g_pAudioMP = 0;

#ifdef __PPC__
	FreeVecPPC(dma->buffer);
#else
	FreeVec(dma->buffer);
#endif
	dma->buffer = 0;

}

#if 0
void PAULA_BeginPainting (void)
{
}

void dma_SplitBuffer(BYTE *pBuffer, BYTE *pLeft, BYTE *pRight, ULONG samples)
{
	int i;

	for (i = 0; i < samples; i++)
	{
		BYTE leftSample	    = *pBuffer++;
		BYTE rightSample    = *pBuffer++;

		*pLeft++	    = leftSample;
		*pRight++	    = rightSample;
	}
}

void PAULA_Submit(void)
{
	/*
	extern int soundtime;
	int start = soundtime % (dma.samples/dma.channels);
	intdata_t *pInt = (intdata_t *)(AudioInt.is_Data);
	BYTE *pBuffer = (BYTE *)dma.buffer + start;
	BYTE *pLeft = (BYTE *)(pInt->m_pBufferLeft + start);
	BYTE *pRight = (BYTE *)(pInt->m_pBufferRight + start);

	if (dma.buffer == 0 || (paintedtime-soundtime) == 0)
		return;

	kprintf("Mixing %d bytes from %d\n", (paintedtime-soundtime), start);

	dma_SplitBuffer(pBuffer, pLeft, pRight, (paintedtime-soundtime));
	*/
}
#endif

/* ========== EXPORTED FUNCTIONS ================================================ */


DLLFUNC sndexport_t GetSndAPI(sndimport_t simp)
{
	sndexport_t se;

	si = simp;

	se.api_version		= SND_API_VERSION;
	se.Init			= PAULA_Init;
	se.Shutdown		= PAULA_Shutdown;
	se.GetDMAPos		= PAULA_GetDMAPos;
	se.BeginPainting	= NULL;
	se.Submit		= NULL;

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
