#pragma amiga-align
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/cybergraphics.h>
#include <proto/graphics.h>
#include <proto/dos.h>
//#include <proto/timer.h>
#include <clib/timer_protos.h>
#include <cybergraphx/cybergraphics.h>
#ifdef __PPC__
#include <powerpc/powerpc.h>
#include <proto/powerpc.h>
#endif
#pragma default-align

#include "/client/client.h"

struct Library *CyberGfxBase = 0;
struct IntuitionBase *IntuitionBase = 0;
struct GfxBase *GfxBase = 0;
struct Device *TimerBase;
struct timerequest *tr;

struct Screen *g_pScreen = 0;
struct Window *g_pWindow = 0;
UBYTE *g_pCursor = 0;
extern int vid_width;
extern int vid_height;
extern int vid_rowbytes;
extern char *vid_buffer2;
unsigned char first_palette[4*256];
int screen_public=0;
unsigned char currentpalette[1024];

void Term();

UWORD word_pal[256];

#define RGB15(r,g,b) (UWORD)( (((r)&0xF8)<<7) | (((g)&0xF8)<<2) | (((b)&0xF8)>>3))
#define RGB16(r,g,b) (UWORD)( (((r)&0xF8)<<8) | (((g)&0xFC)<<3) | (((b)&0xF8)>>3))

unsigned long __stack = 0xFA000;  // auto stack Cowcat

short   ShortSwap (short l)
{
 unsigned char    b1,b2;

   //byte b1,b2;

 b1 = l&255;
 b2 = (l>>8)&255;

 return (b1<<8) + b2;
}


void FS_SetPalette(const unsigned char *palette)
{
    	int i,j;
    	ULONG xpalette[300*3];
    	xpalette[0] = 256<<16;
    	if (!g_pScreen) return;

    	j=1;

    	for (i=0; i<256; i++)
    	{
 		char r,g,b;
 		r = palette[i*4];
 		g = palette[i*4+1];
 		b = palette[i*4+2];
 		xpalette[j++] = (r << 24) | 0xFFFFFF;
 		xpalette[j++] = (g << 24) | 0xFFFFFF;
 		xpalette[j++] = (b << 24) | 0xFFFFFF;
    	}

    	xpalette[j] = 0;

    	LoadRGB32(&(g_pScreen->ViewPort), xpalette);
}

void FS_SetRGB15Palette(const unsigned char *palette, int swap)
{
    	int i;

    	for (i=0; i<256; i++)
    	{
 		unsigned char r,g,b;
 		r = palette[i*4];
 		g = palette[i*4+1];
 		b = palette[i*4+2];
 		word_pal[i] = RGB15(r,g,b);
    	}

    	if (swap)
    	{
 	for (i=0; i<256; i++)  word_pal[i] = ShortSwap(word_pal[i]);
    	}
}

void FS_SetRGB16Palette(const unsigned char *palette, int swap)
{
    	int i;

    	for (i=0; i<256; i++)
    	{
 		unsigned char r,g,b;
 		r = palette[i*4];
 		g = palette[i*4+1];
 		b = palette[i*4+2];
 		word_pal[i] = RGB16(r,g,b);
    	}

    	if (swap)
    	{
 	for (i=0; i<256; i++)  word_pal[i] = ShortSwap(word_pal[i]);
    	}
}

void SWimp_SetPalette( const unsigned char *palette )
{
    	ULONG PixFmt;
    	struct BitMap *bm;

    	if (!g_pWindow)
    	{
		memcpy(first_palette, palette, 4*256);
 		return;
    	}

    	bm = g_pWindow->RPort->BitMap;

    	if ( !palette ) palette = ( const unsigned char * ) currentpalette;

    	if (GetCyberMapAttr(bm, CYBRMATTR_ISCYBERGFX))
    	{
 			PixFmt = GetCyberMapAttr(bm,CYBRMATTR_PIXFMT);
 			if (PixFmt == PIXFMT_LUT8 && screen_public == 0)
		  {
     		FS_SetPalette(palette);
			}
 			else
 			switch(PixFmt)
			{
				 case PIXFMT_RGB15:
     			FS_SetRGB15Palette(palette, 0);
     		 break;
 				 case PIXFMT_RGB15PC:
     			FS_SetRGB15Palette(palette, 1);
     		 break;
 				 case PIXFMT_RGB16:
     			FS_SetRGB16Palette(palette, 0);
     		 break;
 				 case PIXFMT_RGB16PC:
     			FS_SetRGB16Palette(palette, 1);
     		 break;
 				 default:
     //ri.Con_Printf(PRINT_ALL, "Unsupported pixmap format\n");
     		 break;
		   }
    	}
}

void SWimp_EndFrame(void)
{
    struct BitMap *bm;
    if (!g_pWindow) return;
    bm = g_pWindow->RPort->BitMap;

    if (GetCyberMapAttr(bm, CYBRMATTR_ISCYBERGFX))
    {
		 ULONG PixFmt,BytesPerRow;
 		 ULONG BytesPerPix;
 		 UBYTE *base;
 		 APTR handle;

     handle = LockBitMapTags(bm,
      LBMI_PIXFMT,        (ULONG)&PixFmt,
      LBMI_BYTESPERROW,   (ULONG)&BytesPerRow,
      LBMI_BASEADDRESS,   (ULONG)&base,
      LBMI_BYTESPERPIX,   (ULONG)&BytesPerPix,
      TAG_DONE);

 		 if (!handle)
     {
//     ri.Con_Printf(PRINT_ALL, "SWimp_EndFrame: Can't lock bitmap\n");
	     return; // Can't do anything about that.
     }
 // Correct the base address

 base = base
      + BytesPerPix * (g_pWindow->LeftEdge + g_pWindow->BorderLeft)
      + BytesPerRow * (g_pWindow->TopEdge  + g_pWindow->BorderTop);

 if (PixFmt == PIXFMT_LUT8)
 {
     UBYTE *buffer = vid_buffer2;
     int i;

     for (i = 0; i < vid_height; i++)
     {
#ifdef __PPC__
		CopyMemPPC(buffer, base, vid_rowbytes);
#else
    		CopyMem(buffer, base, vid_rowbytes);
#endif
  			buffer += vid_rowbytes;
  			base += BytesPerRow;
     }
 }
 else
 {
//  	printf("Wrong!\n");
    #if 0
     int i,j;
     UBYTE *buffer = vid_buffer2;
     for (i = 0; i < vid_height; i++)
     {
			  UBYTE *b1 = buffer;
  			UBYTE *b2 = base;
  			for (j=0; j < vid_width; j++)
			  {
		      *(UWORD *)base = word_pal[*buffer++];
		      base += BytesPerPix;
			  }
  			buffer = b1 + vid_rowbytes;
  			base   = b2 + BytesPerRow;
     }
    #endif
 }

 UnLockBitMap(handle);
    }

    else
    {
// ri.Con_Printf(PRINT_ALL, "SWimp_EndFrame: No CyberGraphX-Compatible bitmap\n");
    }
}

void Init()
{
    CyberGfxBase = OpenLibrary("cybergraphics.library", 0);

    if (!CyberGfxBase)
    {
	exit(20);
    }

    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 0);
    GfxBase       = (struct GfxBase *)OpenLibrary("graphics.library", 0);

    #ifdef __PPC__
    tr = (struct timerequest *)AllocVecPPC(sizeof(struct timerequest), MEMF_CLEAR|MEMF_PUBLIC,0);
    #else
    tr = (struct timerequest *)AllocVec(sizeof(struct timerequest), MEMF_CLEAR|MEMF_PUBLIC);
    #endif

    if (!tr)
    {
	Term();
	exit(20);
    }

    if (OpenDevice(TIMERNAME, UNIT_MICROHZ, (struct IORequest *)tr, 0) != 0)
    {
	Term();
	exit(20);
    }

    TimerBase = (struct Device *)tr->tr_node.io_Device;
}

void Term()
{
    if (CyberGfxBase)   CloseLibrary(CyberGfxBase);
    if (IntuitionBase)  CloseLibrary((struct Library *)IntuitionBase);
    if (GfxBase)        CloseLibrary((struct Library *)GfxBase);
    if (TimerBase)      CloseDevice((struct IORequest *)tr);

    #ifdef __PPC__
    if (tr)             FreeVecPPC(tr);
    #else
    if (tr)		FreeVec(tr);
    #endif

    CyberGfxBase = 0;
    IntuitionBase = 0;
    GfxBase = 0;
    TimerBase = 0;
    tr = 0;
}


unsigned int timeGetTime()
{
    static struct timeval tv;

    #ifdef __PPC__
    GetSysTimePPC(&tv);    // Read the Amiga time
    #else
    GetSysTime(&tv);
    #endif

    return  (unsigned long)tv.tv_secs*1000
		  + (unsigned long)(tv.tv_micro/1000);
}

ULONG GetMode(int w, int h)
{
    ULONG modeID, error, result;
    struct DisplayInfo dpinfo;
    struct NameInfo nminfo;
    struct DimensionInfo diminfo;
    DisplayInfoHandle displayhandle;

    ULONG bestMode = INVALID_ID, bestW=0xffffffff, bestH = 0xffffffff;
    ULONG nW, nH;


    modeID = INVALID_ID;

    GetDisplayInfoData(NULL, (UBYTE *)&dpinfo, sizeof(struct DisplayInfo),
	    DTAG_DISP, LORES_KEY);

    while ((modeID = NextDisplayInfo(modeID)) != INVALID_ID)
    {
	if (IsCyberModeID(modeID) == FALSE)
	    continue;

	error = ModeNotAvailable(modeID);

	if (error == 0)
	{
	    displayhandle = FindDisplayInfo(modeID);
	    if (displayhandle)
	    {
		result = GetDisplayInfoData(displayhandle, (UBYTE *)&diminfo,
			    sizeof(struct DimensionInfo), DTAG_DIMS, 0);
		if (result)
		{
		    if (diminfo.MaxDepth == 8)
		    {
			nW = diminfo.Nominal.MaxX - diminfo.Nominal.MinX + 1;
			nH = diminfo.Nominal.MaxY - diminfo.Nominal.MinY + 1;

			if (nW == h && nH == w)
			{
			    // Exact match
			    return modeID;
			}
			else
			{
			    // Check if the widht and height are big enough to even take the screen
			    if (nW >= w && nH >= h)
			    {
				// Yes. Check if they match the requested size better
				if (nW <= bestW && nH <= bestH)
				{
				    // They do
				    bestW = nW;
				    bestH = nH;
				    bestMode = modeID;
				}
			    }
			}
		    }
		}
	    }
	}
    }


    // Here, we either have a mode that roughly matches, or none
    return bestMode;
}

BOOL OpenDisplay(ULONG Width, ULONG Height)
{
    ULONG ModeID = GetMode(Width, Height);

    if (ModeID != INVALID_ID)
    {
	if (g_pScreen = OpenScreenTags(NULL,
			    SA_DisplayID,       ModeID,
			    SA_Width,           Width,
			    SA_Height,          Height,
			    SA_Depth,           8,
			    SA_ShowTitle,       FALSE,
			    SA_Quiet,           TRUE,
			    SA_FullPalette,     TRUE,
			TAG_DONE))
	{
	    if (g_pWindow = OpenWindowTags(NULL,
				WA_Left,        0,
				WA_Top,         0,
				WA_Flags,       WFLG_BORDERLESS|WFLG_NOCAREREFRESH|WFLG_ACTIVATE|WFLG_RMBTRAP,
				WA_IDCMP,       IDCMP_RAWKEY,
				WA_InnerWidth,  Width,
				WA_InnerHeight, Height,
				WA_CustomScreen,(ULONG)g_pScreen,
			    TAG_DONE))
	    {
                #ifdef __PPC__
		if (g_pCursor = AllocVecPPC(24, MEMF_CLEAR|MEMF_CHIP,0))
                #else
	        if (g_pCursor = AllocVec(24, MEMF_CLEAR|MEMF_CHIP))
		#endif
		{
		    SetPointer(g_pWindow, (UWORD*)g_pCursor, 1, 16, 0, 0);
		    return TRUE;
		}

		CloseWindow(g_pWindow);
	    }

	    CloseScreen(g_pScreen);
	}
    }

    return FALSE;
}


void CloseDisplay()
{
    if (g_pWindow)      CloseWindow(g_pWindow);
    if (g_pScreen)      CloseScreen(g_pScreen);
    #ifdef __PPC__
    if (g_pCursor)      FreeVecPPC(g_pCursor);
    #else
    if (g_pCursor)      FreeVec(g_pCursor);
    #endif
}


void MainLoop()
{
    struct MsgPort *pPort = 0;
    struct IntuiMessage *pMsg;
    BOOL bRunning = TRUE;

    if (g_pWindow)
	pPort = g_pWindow->UserPort;

    if (!pPort)
	return;

    //while (bRunning)
    {
	// Decode and display frame
	//Delay(1);
	while(pMsg = (struct IntuiMessage *)GetMsg(pPort))
	{
	    if (pMsg->Class == IDCMP_RAWKEY)
    {
		bRunning = FALSE;
  		CloseDisplay();
    		Term();
    		S_Shutdown();
		exit(1);

		}
	    ReplyMsg((struct Message *)pMsg);
	}
    }
}

void S_Init();
void S_Shutdown();

extern int use_paula;

extern cvar_t ahi_speed,ahi_channels,ahi_bits,s_khz;

extern float vid_gamma;
extern float s_volume;

void init_params(int argc,char **argv)
{
  if (!strcmp(argv[2],"paula")) use_paula=1;
  else use_paula=0;

	ahi_speed.value	  =atoi(argv[3]);
  	ahi_channels.value=atoi(argv[4]);
  	ahi_bits.value	  =atoi(argv[5]);
  	s_khz.value	  =atoi(argv[6]);

	if (argc>7) vid_gamma=atof(argv[7]);
	else vid_gamma=1.0f;

	if (argc>8) s_volume=atof(argv[8]);
  	else s_volume=0.7f;
}

int main(int argc, char **argv)
{
    
    if (argc < 7)
	return 20;

	init_params(argc,argv);

    	S_Init();
    	Init();

    //printf("Playing %s\n", argv[1]);
    	SCR_PlayCinematic(argv[1]);

	CloseDisplay();
    	Term();
    	S_Shutdown();

    return 0;
}
