/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
/*
** RW_IMP.C
**
** This file contains ALL Amiga specific stuff having to do with the
** software refresh.  When a port is being made the following functions
** must be implemented by the port:
**
** SWimp_EndFrame
** SWimp_Init
** SWimp_SetPalette
** SWimp_Shutdown
*/

#include "../ref_soft/r_local.h"

#ifdef __VBCC__
#pragma amiga-align
#elif defined(WARPUP)
#pragma pack(push,2)
#endif

#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/gfx.h>
#include <cybergraphx/cybergraphics.h>
#include <exec/exec.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>

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
#elif defined (WARPUP)
#pragma pack(pop)
#endif

#include "dll.h"

#define P96FIX	1

struct Library *CyberGfxBase;
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

static qboolean libs_init = false;
static qboolean screen_public = true;

struct Window 	*g_pWindow;
struct Screen 	*g_pScreen;

UWORD 		word_pal[256];
unsigned char 	first_palette[4*256];

#define RGB16(r,g,b) (UWORD)( (((r)&0xF8)<<8) | (((g)&0xFC)<<3) | (((b)&0xF8)>>3))
#define RGB15(r,g,b) (UWORD)( (((r)&0xF8)<<7) | (((g)&0xF8)<<2) | (((b)&0xF8)>>3))

#define MAYBE_OPEN_LIBS if (libs_init == false) SWimp_OpenLibs()

qboolean SWimp_OpenLibs(void)
{    
    	if (libs_init)
		return true; 
    
    	libs_init = true;

    	CyberGfxBase = OpenLibrary("cybergraphics.library", 0L);

    	if (!CyberGfxBase)
		return false;

    	IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 0L);

    	if (!IntuitionBase)
		return false; 

    	GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 0L);

    	if (!GfxBase)
		return false; 

    	return true;
}

void SWimp_CloseLibs(void)
{    
	if (libs_init == false) return;
	if (CyberGfxBase) CloseLibrary(CyberGfxBase);
	if (IntuitionBase) CloseLibrary((struct Library *)IntuitionBase);
	if (GfxBase) CloseLibrary((struct Library *)GfxBase);

	CyberGfxBase = NULL;
	IntuitionBase = NULL;
	GfxBase = NULL;
	libs_init = false;
}

static void *g_pCursor = 0;

DLLFUNC void SWimp_EmptyCursor(void)
{	
	if (!g_pCursor)
	{
		#ifdef __PPC__

		g_pCursor = AllocVecPPC(24, MEMF_CLEAR|MEMF_CHIP,0);

		#else

		g_pCursor = AllocVec(24, MEMF_CLEAR|MEMF_CHIP);

		#endif
		
		SetPointer(g_pWindow, g_pCursor, 1, 16, 0, 0);
		
	}
}

DLLFUNC void SWimp_EnableCursor(void)
{
	if (g_pCursor)
	{
		ClearPointer(g_pWindow);

		#ifdef __PPC__

		FreeVecPPC(g_pCursor);

		#else

		FreeVec(g_pCursor);

		#endif	  

		g_pCursor = 0;
	}
}

/*
** VID_CreateWindow
*/

#define WINDOW_CLASS_NAME "Quake 2"

ULONG GetMode(int w, int h);

qboolean VID_CreateWindow( int width, int height, qboolean fullscreen)
{
	cvar_t	*vid_xpos, *vid_ypos, *vid_fullscreen, *sw_pubscreen, *sw_keepcursor;
	ULONG	wflgs;
	Tag	screentag;
	Tag	titletag;
	int	x,y;

	MAYBE_OPEN_LIBS;

	vid_xpos = ri.Cvar_Get ("vid_xpos", "0", 0);
	vid_ypos = ri.Cvar_Get ("vid_ypos", "0", 0);
	vid_fullscreen = ri.Cvar_Get("vid_fullscreen", "0", CVAR_ARCHIVE );
	sw_pubscreen   = ri.Cvar_Get("sw_pubscreen", "", CVAR_ARCHIVE);
	sw_keepcursor  = ri.Cvar_Get("sw_keepcursor", "0", CVAR_ARCHIVE);

	if ( vid_fullscreen->value || fullscreen)
	{
		#ifndef P96FIX

		// Open a screen for fullscreen mode
		struct TagItem tags[] =
		{
			{CYBRBIDTG_Depth,	    8},
			{CYBRBIDTG_NominalWidth,    0},
			{CYBRBIDTG_NominalHeight,   0},
			{TAG_DONE,		    0}
		};

		ULONG ModeID;
		ULONG errorCode;

		tags[1].ti_Data = width;
		tags[2].ti_Data = height;

		ModeID = BestCModeIDTagList(&tags);

		#else

		ULONG errorCode;
		ULONG ModeID = GetMode(width, height);

		#endif

		g_pScreen = OpenScreenTags(NULL,
			SA_DisplayID,		ModeID,
			SA_Width,		width,
			SA_Height,		height,
			SA_Depth,		8,
			SA_ShowTitle,		FALSE,
			SA_Quiet,		TRUE,
			SA_FullPalette,		TRUE,
			SA_ErrorCode,		(ULONG)&errorCode,
			TAG_DONE);

		if (!g_pScreen)
		{
			ri.Con_Printf(PRINT_ALL, "OpenScreen: ");

			switch(errorCode)
			{
				case OSERR_NOMONITOR:
					ri.Con_Printf(PRINT_ALL, "Monitor for display mode not available\n");
					break;

				case OSERR_NOCHIPS:
					ri.Con_Printf(PRINT_ALL, "Custom chips for display mode missing\n");
					break;

				case OSERR_NOMEM:
					ri.Con_Printf(PRINT_ALL, "Out of system memory\n");
					break;

				case OSERR_NOCHIPMEM:
					ri.Con_Printf(PRINT_ALL, "Out of chip memory\n");
					break;

				case OSERR_PUBNOTUNIQUE:
					ri.Con_Printf(PRINT_ALL, "Public screen name already in use\n");
					break;

				case OSERR_UNKNOWNMODE:
					ri.Con_Printf(PRINT_ALL, "Didn't recognize display mode requested\n");
					break;

				case OSERR_TOODEEP:
					ri.Con_Printf(PRINT_ALL, "This hardware cannot display screens of that depth\n");
					break;

				case OSERR_ATTACHFAIL:
					ri.Con_Printf(PRINT_ALL, "An illegal attachment of screens was requested\n");
					break;

				default:
					ri.Con_Printf(PRINT_ALL, "Error code %s\n", errorCode);
					break;
			}

			return false;
		}
		
		screen_public = false;
	}

	else
	{		
		if (sw_pubscreen->string && strlen(sw_pubscreen->string))
			g_pScreen = LockPubScreen((UBYTE *)sw_pubscreen->string);

		else
			g_pScreen = LockPubScreen(NULL);

		if (g_pScreen == NULL)
		{
			g_pScreen = LockPubScreen(NULL); // Fall back to default

			if (g_pScreen == NULL)
				return false;
		}
	    
		screen_public = true;
	}

	// At this point g_pScreen is either a pubscreen pointer or a custom screen
	if (screen_public == true)
	{
		screentag = WA_PubScreen;
		titletag = WA_Title;
		wflgs = WFLG_REPORTMOUSE | WFLG_NOCAREREFRESH | WFLG_ACTIVATE | WFLG_RMBTRAP | WFLG_DRAGBAR;
	}

	else
	{
		screentag = WA_CustomScreen;
		titletag = TAG_IGNORE;
		wflgs = WFLG_BORDERLESS | WFLG_REPORTMOUSE | WFLG_NOCAREREFRESH | WFLG_ACTIVATE | WFLG_RMBTRAP;
	}

	if (vid_xpos && vid_xpos->value)
	    x = vid_xpos->value;

	else
	    x = 0;

	if (vid_ypos && vid_ypos->value)
	    y = vid_ypos->value;

	else
	    y = 0;

	g_pWindow = OpenWindowTags(NULL,
		WA_Left,	    (ULONG)x,
		WA_Top,		    (ULONG)y,
		WA_Flags,	    wflgs,
		WA_InnerWidth,	    width,
		WA_InnerHeight,	    height,
		screentag,	    (ULONG)g_pScreen,
		titletag,	    (ULONG)"Quake2",
		TAG_DONE);

	if (!g_pWindow)
		ri.Sys_Error (ERR_FATAL, "Couldn't create window"); 

	// let the sound and input subsystems know about the new window
	ri.Vid_NewWindow (width, height);
	
	SWimp_SetPalette(first_palette);
	SWimp_EmptyCursor();

	return true;
}

/*
** SWimp_Init
**
** This routine is responsible for initializing the implementation
** specific stuff in a software rendering subsystem.
*/

int SWimp_Init( void *hInstance, void *wndProc )
{   
	MAYBE_OPEN_LIBS;

	#if 0 // Cowcat

	if (!g_pCursor)

	#ifdef __PPC__	  
		g_pCursor = AllocVecPPC(24, MEMF_CLEAR|MEMF_CHIP,0);
	#else
		g_pCursor = AllocVec(24, MEMF_CLEAR|MEMF_CHIP);
	#endif

	#endif ///
 
	SWimp_EnableCursor();  // Cowcat

	return true;
}

/*
** SWimp_InitGraphics
**
** This initializes the software refresh's implementation specific
** graphics subsystem.	In the case of Windows it creates DIB or
** DDRAW surfaces.
**
** The necessary width and height parameters are grabbed from
** vid.width and vid.height.
*/

static qboolean SWimp_InitGraphics( qboolean fullscreen )
{
	// free resources in use
	SWimp_Shutdown ();

	// create a new window
	VID_CreateWindow (vid.width, vid.height, fullscreen);

	vid.buffer	= malloc(vid.width * vid.height);
	vid.rowbytes	= vid.width;
	
	return true;
}

/*
** SWimp_EndFrame
**
** This does an implementation specific copy from the backbuffer to the
** front buffer.  In the Amiga case it uses LockBitMap to get the bitmap pointer,
** and then tries to either update directly in case of LUT8 or indirectly via a
** lookup table otherwise.
*/

DLLFUNC void SWimp_EndFrame(void)
{
	struct BitMap *bm;

	if (!g_pWindow)
		return;

	bm = g_pWindow->RPort->BitMap;

	//if (GetCyberMapAttr(bm, CYBRMATTR_ISCYBERGFX))
	{
		#if 1

		ULONG PixFmt, BytesPerRow, BytesPerPix;
		UBYTE *base;
		APTR handle;

		handle = LockBitMapTags(bm,
			LBMI_PIXFMT,	    (ULONG)&PixFmt,
			LBMI_BYTESPERROW,   (ULONG)&BytesPerRow,
			LBMI_BASEADDRESS,   (ULONG)&base,
			LBMI_BYTESPERPIX,   (ULONG)&BytesPerPix,
			TAG_DONE);

		#else

		ULONG PixFmt, BytesPerRow, BytesPerPix;
		UBYTE *base;
		APTR handle;

		struct TagItem bitmaptags[] =
		{
			{ LBMI_PIXFMT,	    (ULONG)&PixFmt },
			{ LBMI_BYTESPERROW,   (ULONG)&BytesPerRow },
			{ LBMI_BASEADDRESS,   (ULONG)&base },
			{ LBMI_BYTESPERPIX,   (ULONG)&BytesPerPix },
			{ TAG_DONE,		0 }
		};

		handle = LockBitMapTagList(bm, bitmaptags);

		#endif

		if (!handle)
		{
			ri.Con_Printf(PRINT_ALL, "SWimp_EndFrame: Can't lock bitmap\n");
			return; // Can't do anything about that.
		}

		// Correct the base address
		base = base
			+ BytesPerPix * (g_pWindow->LeftEdge + g_pWindow->BorderLeft)
			+ BytesPerRow * (g_pWindow->TopEdge  + g_pWindow->BorderTop);

		if (PixFmt == PIXFMT_LUT8)
		{
			UBYTE *buffer = vid.buffer;
			int i;

			for (i = 0; i < vid.height; i++)
			{
				#ifdef __PPC__	    
				CopyMemPPC(buffer, base, vid.rowbytes);
				#else
				CopyMem(buffer, base, vid.rowbytes);
				#endif
   
				buffer += vid.rowbytes;
				base += BytesPerRow;
			}
		}

		else
		{
			int i, j;
			UBYTE *buffer = vid.buffer;

			for (i = 0; i < vid.height; i++)
			{
				UBYTE *b1 = buffer;
				UBYTE *b2 = base;

				for (j=0; j < vid.width; j++)
				{
					*(UWORD *)base = word_pal[*buffer++];
					base += BytesPerPix;
				}

				buffer = b1 + vid.rowbytes;
				base   = b2 + BytesPerRow;
			}
		}

		UnLockBitMap(handle);
	}

	//else
	{
		//ri.Con_Printf(PRINT_ALL, "SWimp_EndFrame: No CyberGraphX-Compatible bitmap\n");
	}
	
}

/*
** SWimp_SetMode
*/

rserr_t SWimp_SetMode( int *pwidth, int *pheight, int mode, qboolean fullscreen )
{	
	const char *win_fs[] = { "W", "FS" };
	rserr_t retval = rserr_ok;	

	ri.Con_Printf (PRINT_ALL, "setting mode %d:", mode );

	if ( !ri.Vid_GetModeInfo( pwidth, pheight, mode ) )
	{
		ri.Con_Printf( PRINT_ALL, " invalid mode\n" );
		return rserr_invalid_mode;
	}

	ri.Con_Printf( PRINT_ALL, " %d %d %s\n", *pwidth, *pheight, win_fs[fullscreen] );

	if ( fullscreen )
	{
		if ( !SWimp_InitGraphics( 1 ) )
		{
			if ( SWimp_InitGraphics( 0 ) )
			{
				// mode is legal but not as fullscreen
				fullscreen = 0;
				retval = rserr_invalid_fullscreen;
			}

			else
			{
				// failed to set a valid mode in windowed mode
				retval = rserr_unknown;
			}
		}
	}

	else
	{
		// failure to set a valid mode in windowed mode
		if ( !SWimp_InitGraphics( fullscreen ) )
		{
			return rserr_unknown;
		}
	}

	sw_state.fullscreen = fullscreen;
	R_GammaCorrectAndSetPalette( ( const unsigned char * ) d_8to24table );

	return retval;
}

void FS_SetPalette(const unsigned char *palette)
{    
	int 	i, j;
	ULONG 	xpalette[300*3];

	xpalette[0] = 256<<16;

	if (!g_pScreen)
		return;

	j = 1;

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

static short __ShortSwap (short l)
{
	byte	b1,b2;

	b1 = l&255;
	b2 = (l>>8)&255;

	return (b1<<8) + b2;
}

void FS_SetRGB15Palette(const unsigned char *palette, qboolean swap)
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

	if (swap == true)
	{
		for (i=0; i<256; i++)
			word_pal[i] = __ShortSwap(word_pal[i]);
	}
}

void FS_SetRGB16Palette(const unsigned char *palette, qboolean swap)
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

	if (swap == true)
	{
		for (i=0; i<256; i++)
			word_pal[i] = __ShortSwap(word_pal[i]);
	}
}


/*
** SWimp_SetPalette
**
** System specific palette setting routine.  A NULL palette means
** to use the existing palette.	 The palette is expected to be in
** a padded 4-byte xRGB format.
*/

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
    
	if ( !palette )
		palette = ( const unsigned char * ) sw_state.currentpalette;
     
	if (GetCyberMapAttr(bm, CYBRMATTR_ISCYBERGFX))
	{
		PixFmt = GetCyberMapAttr(bm,CYBRMATTR_PIXFMT);

		if (PixFmt == PIXFMT_LUT8 && screen_public == false)
		{
			FS_SetPalette(palette);
		}

		else
			switch(PixFmt)
			{
				case PIXFMT_RGB15:
					FS_SetRGB15Palette(palette, false);
					break;

				case PIXFMT_RGB15PC:
					FS_SetRGB15Palette(palette, true);
					break;

				case PIXFMT_RGB16:
					FS_SetRGB16Palette(palette, false);
					break;

				case PIXFMT_RGB16PC:
					FS_SetRGB16Palette(palette, true);
					break;

				default:
					ri.Con_Printf(PRINT_ALL, "Unsupported pixmap format\n");
					break;
			}
	}

}

/*
** SWimp_Shutdown
**
** System specific graphics subsystem shutdown routine.	 Destroys
** DIBs or DDRAW surfaces as appropriate.
*/

void SWimp_Shutdown( void )
{
	ri.Con_Printf( PRINT_ALL, "Shutting down SW imp\n" );

	if (g_pWindow)
		CloseWindow(g_pWindow);

	g_pWindow = 0;

	if (g_pScreen)
	{
		if (screen_public == false)
			CloseScreen(g_pScreen);
 
		else
			UnlockPubScreen(NULL, g_pScreen);
	}

	g_pScreen = 0;

	screen_public = false;

	#if 0 // Cowcat
    
	#ifdef __PPC__	  
	if (g_pCursor) FreeVecPPC(g_pCursor);
	#else
	if (g_pCursor) FreeVec(g_pCursor);
	#endif	  
	g_pCursor = 0;

	#endif ////

	SWimp_EnableCursor();
}

/*
** SWimp_AppActivate
*/

DLLFUNC void SWimp_AppActivate( qboolean active )
{
}

DLLFUNC struct Window *GetWindowHandle(void)
{
	return g_pWindow;
}

//===============================================================================

#if 0
DLLFUNC void *dllFindResource(int id, char *pType)
{
	return NULL;
}

DLLFUNC void *dllLoadResource(void *pHandle)
{
	return NULL;
}

DLLFUNC void dllFreeResource(void *pHandle)
{
	return;
}
#endif

extern refexport_t GetRefAPI(refimport_t rimp);

dll_tExportSymbol DLL_ExportSymbols[] =
{
	//{(void *)dllFindResource,"dllFindResource"}, // (void *) Cowcat
	//{(void *)dllLoadResource,"dllLoadResource"},
	//{(void *)dllFreeResource,"dllFreeResource"},
	{(void *)GetRefAPI,"GetRefAPI"},
	{(void *)GetWindowHandle, "GetWindowHandle"},
	{0, 0},
};

dll_tImportSymbol DLL_ImportSymbols[] =
{
    	{0, 0, 0, 0}
};

int DLL_Init(void)
{
    	return 1L;
}

void DLL_DeInit(void)
{
}

#ifdef __GNUC__
extern int main(int, char **); // new Cowcat
#endif

ULONG GetMode(int w, int h)
{   
	ULONG modeID, error,result;
	struct DisplayInfo dpinfo;
	//struct NameInfo nminfo;
	struct DimensionInfo diminfo;
	DisplayInfoHandle displayhandle;
    
	ULONG bestMode = INVALID_ID, bestW=0xffffffff, bestH = 0xffffffff;
	ULONG nW, nH;

	modeID = INVALID_ID;
    
	GetDisplayInfoData(NULL, (UBYTE *)&dpinfo, sizeof(struct DisplayInfo), DTAG_DISP, LORES_KEY);

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
				result = GetDisplayInfoData(displayhandle, (UBYTE *)&diminfo, sizeof(struct DimensionInfo), DTAG_DIMS, 0); 
					  //, DTAG_DIMS, NULL) ?? problem compiling Cowcat 

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
					} // if (diminfo...
				} // if (result...
			} // if (displayhandle...
		} // if (error...
	
	} // while
	
	// Here, we either have a mode that roughly matches, or none	
	return bestMode;
}
