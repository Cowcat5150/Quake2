//     ___	 ___
//   _/	 /_______\  \_	   ___ ___ __ _			      _ __ ___ ___
//__//	/ _______ \  \\___/						  \___
//_/ | '  \__ __/  ` | \_/	  © Copyright 1998, Christopher Page	   \__
// \ | |    | |__  | | / \		 All Rights Reserved		   /
//  >| .    |  _/  . |<	  >--- --- -- -			      - -- --- ---<
// / \	\   | |	  /  / \ / This file may not be distributed, reproduced or \
// \  \	 \_/   \_/  /  / \  altered, in full or in part, without written   /
//  \  \	   /  /	  \    permission from Christopher Page. Legal	  /
// //\	\_________/  /\\ //\	 action will be taken in cases where	 /
//¯ ¯¯\	  _______   /¯¯ ¯ ¯¯\	     this notice is not obeyed.		/¯¯¯¯¯
//¯¯¯¯¯\_/	 \_/¯¯¯¯¯¯¯¯¯\	 ___________________________________   /¯¯¯¯¯¯
//			      \_/				    \_/
//
// Description:
//
//  CD playback functions
//
// Functions:
//
//  struct TWFCDData * TWFCD_Setup(REG(a0) STRPTR DeviceName, REG(d0) UBYTE UnitNum)
//  ULONG TWFCD_Shutdown(REG(a0) struct TWFCDData *tcs_OldData)
//  ULONG TWFCD_ReadTOC(REG(a0) struct TWFCDData *tcrt_CDData)
//  ULONG TWFCD_PlayTracks(REG(a0) struct TWFCDData *tcpt_CDData, REG(a1) struct TagItem *pt_Tags)
//  ULONG TWFCD_StopPlay(REG(a0) struct TWFCDData *tcsp_CDData)
//  ULONG TWFCD_PausePlay(REG(a0) struct TWFCDData *tcpp_CDData)
//  ULONG TWFCD_MotorControl(REG(a0) struct TWFCDData *tcet_CDData, REG(d0) UBYTE newStatus)
//  ULONG TWFCD_ReadSubChannel(REG(a0) struct TWFCDData *tcrs_CDData)
//  static ULONG TWFCD_DoSCSICmd(struct TWFCDData *tcdc_CDData, UBYTE *buffer, int bufferSize, UBYTE *cmd, int cmdSize, UBYTE flags)
//
// Detail:
//
//  This code provides a simple method to query the contents of a CD and start, stop
//  and pause playback of audio tracks on that CD. This is still work in progress and
//  likely to have problems on some non-standard/ older CD-ROM models.
//
//
// Version:
//
//  $VER: twfsound_cd.c 3.18 (17/10/1999)
//
// Fold Markers:
//
//  Start: /*GFS*/
//    End: /*GFE*/
//
// Coded by and received from "The World Foundry",
// (chris@worldfoundry.demon.co.uk)
// This code is *not* under GPL, but it can be used for Freeware and
// commercial projects. It is allowed to include the source-code, but
// it is not required (Unless the game in question - like Quake -
// requires to release the source). The release of this code inside
// a GPL project like Quake does not put other projects which use
// this code GPL, as this CD Audio Replay Code is not GPL, despite the
// fact that the source code is allowed to be included.
// Despite being freeware, source code modifications in this source
// require the agreement of TWF. This especially includes also
// recompiles to different Kernels.

/*
	    Portability
	    ===========

	    This file includes support for WarpUP and 68k. In case of
	    PowerUP the AllocVecPPC/FreeVecPPC calls would be needed
	    to be replaced by alternative functions. I am not sure
	    if a modification to compile this with PowerUP would be
	    agreed by the original authors. It is possible that this
	    might be a problem. But if someone wants to compile Quake
	    for PowerUP - i personally doubt that this is worth the
	    effort for anyone - he has to check this out with TWF,
	    or look for a different CD Audio Replay Solution. I did
	    not ask TWF about this, as I personally don't care if a
	    PowerUP version will be done or not. Some includes would
	    need to be changed for different compilers, like usual.

	    Steffen Haeuser (MagicSN@Birdland.es.bawue.de)
*/

/*
 * Additional changes, as required for our Quake port, and
 * PowerUp support by Frank Wille <frank@phoenix.owl.de> (phx)
 */

#ifdef __VBCC__
#pragma amiga-align
#elif defined(WARPUP)
#pragma pack(push,2)
#endif

#include <exec/exec.h>
#include <proto/exec.h>
#include <devices/trackdisk.h>
//#include <proto/utility.h>

#ifdef __PPC__
#if defined(__GNUC__)
#include <powerpc/powerpc.h>
#include <powerpc/powerpc_protos.h>
#else
#include <powerpc/powerpc.h>
#include <proto/powerpc.h>
#endif

#ifdef __VBCC__
#pragma default-align
#elif defined(WARPUP)
#pragma pack(pop)
#endif
#endif

#include "twfsound_cd.h"
#include <stdio.h>

//struct UtilityBase *UtilityBase; // Cowcat 
struct Library *UtilityBase;

// Non-standard, and a bit of a cheat, but it makes the code easier to read
struct TOCData
{
	UBYTE Empty1;  // Padding
	UBYTE Flags ;
	UBYTE Track ;
	UBYTE Empty2;
	ULONG Addr  ;
};

struct SubQData
{
	UBYTE Reserved	 ; // Header: bytes 0 to 3
	UBYTE AudioStatus;
	UWORD DataLength ;
	UBYTE Formatcode ; // Data: byte 4
	UBYTE AdrControl ; // Byte 5
	UBYTE TrackNum	 ; // Byte 6
	UBYTE IndexNum	 ; // Byte 7
	ULONG AbsoluteAdr; // Bytes  8,	 9, 10, 11
	ULONG RelativeAdr; // Bytes 12, 13, 14, 15
};

/* (phx) @@@?  use -DNDEBUG instead! */
/* #define kprintf */

static ULONG TWFCD_DoSCSICmd(struct TWFCDData *, UBYTE *, int , UBYTE *, int, UBYTE);

/* ULONG TWFCD_Setup(STRPTR, UBYTE)					     */
/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=					     */
/* This routine creates the message port and IO structure needed by the	     */
/* device, then it attepmts to open the device.. nothig really groundbreaking*/
/* in here you know...							     */

/*GFS*/ struct TWFCDData *TWFCD_Setup(STRPTR DeviceName, UBYTE UnitNum)
{
	struct TWFCDData *NewData   = NULL;
	BYTE		 DeviceCode = 0;

	if (!UtilityBase)
		UtilityBase=(struct Library *)OpenLibrary("utility.library",0);
    
	// create the table...

	#ifdef __PPC__

	if(!(NewData = AllocVecPPC(sizeof(struct TWFCDData), MEMF_ANY|MEMF_CLEAR,0)))
		return(NULL);

	(NewData->TWFCD_Cmd).cdc_SCSIData = AllocVecPPC(CDBUFFER_SIZE,MEMF_CHIP|MEMF_CLEAR,0);

	#else

	if(!(NewData = AllocVec(sizeof(struct TWFCDData), MEMF_ANY|MEMF_CLEAR)))
		return(NULL);

	(NewData->TWFCD_Cmd).cdc_SCSIData = AllocVec(CDBUFFER_SIZE,MEMF_CHIP|MEMF_CLEAR);

	#endif

	if (!((NewData->TWFCD_Cmd).cdc_SCSIData))
	{
		#ifdef __PPC__

		FreeVecPPC(NewData);

		#else

		FreeVec(NewData);

		#endif

		return 0;
	}

	#ifdef __PPC__

	(NewData->TWFCD_Cmd).cdc_SCSISense = AllocVecPPC(CDSENSE_SIZE,MEMF_CHIP|MEMF_CLEAR,0);

	if (!((NewData->TWFCD_Cmd).cdc_SCSISense))
	{
		FreeVecPPC((NewData->TWFCD_Cmd).cdc_SCSIData);
		FreeVecPPC(NewData);
		return 0;
	}

	(NewData->TWFCD_Cmd).cdc_TOCBuffer = AllocVecPPC(CDTOC_SIZE,MEMF_CHIP|MEMF_CLEAR,0);

	if (!((NewData->TWFCD_Cmd).cdc_TOCBuffer))
	{
		FreeVecPPC((NewData->TWFCD_Cmd).cdc_SCSIData);
		FreeVecPPC((NewData->TWFCD_Cmd).cdc_SCSISense);
		FreeVecPPC(NewData);
	}

	#else

	(NewData->TWFCD_Cmd).cdc_SCSISense = AllocVec(CDSENSE_SIZE,MEMF_CHIP|MEMF_CLEAR);

	if (!((NewData->TWFCD_Cmd).cdc_SCSISense))
	{
		FreeVec((NewData->TWFCD_Cmd).cdc_SCSIData);
		FreeVec(NewData);
		return 0;
	}

	(NewData->TWFCD_Cmd).cdc_TOCBuffer=AllocVec(CDTOC_SIZE,MEMF_CHIP|MEMF_CLEAR);

	if (!((NewData->TWFCD_Cmd).cdc_TOCBuffer))
	{
		FreeVec((NewData->TWFCD_Cmd).cdc_SCSIData);
		FreeVec((NewData->TWFCD_Cmd).cdc_SCSISense);
		FreeVec(NewData);
	}

	#endif

	// Create the reply MsgPort for the device
	if(!(NewData -> TWFCD_Cmd.cdc_MsgPort = (struct MsgPort *)CreateMsgPort()))
		return(NULL);

	// Create our IORequest, to send Play IO messages to the CD device
	NewData -> TWFCD_Cmd.cdc_IOStdReq = (struct IOStdReq *)CreateIORequest(NewData -> TWFCD_Cmd.cdc_MsgPort, sizeof(struct IOStdReq));

	if(NewData -> TWFCD_Cmd.cdc_IOStdReq == NULL)
		return (NULL);

	// Open the CD device for the Play commands
	DeviceCode = OpenDevice(DeviceName, UnitNum, (struct IORequest *)NewData -> TWFCD_Cmd.cdc_IOStdReq, 0);

	if(DeviceCode != 0)
		return(NULL);

	((NewData->TWFCD_Cmd).cdc_IOStdReq)->io_Command = CMD_START;

	DoIO((struct IORequest *)((NewData->TWFCD_Cmd).cdc_IOStdReq));

	// Device is now open, CD play available.
	return(NewData);

} /*GFE*/


/* ULONG TWFCD_Shutdown(TWFCDData *)					     */
/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-					     */
/* Call this to close the CD playback device, IO struct and message port     */
/* allocated by the TWFCD_Setup routine.				     */

/*GFS*/ ULONG TWFCD_Shutdown(struct TWFCDData *tcs_OldData)
{
	if(!tcs_OldData)
		return(TWFCD_FAIL);

	// Halt playback (so few people bother doing this.....)
	if(tcs_OldData -> TWFCD_Cmd.cdc_IOStdReq)
		TWFCD_StopPlay(tcs_OldData);

	// Kill memory allocation
	if(tcs_OldData -> TWFCD_Cmd.cdc_IOStdReq)
		CloseDevice((struct IORequest*)tcs_OldData -> TWFCD_Cmd.cdc_IOStdReq);

	if(tcs_OldData -> TWFCD_Cmd.cdc_IOStdReq)
	{
		DeleteIORequest((struct IORequest*)tcs_OldData -> TWFCD_Cmd.cdc_IOStdReq);
		tcs_OldData -> TWFCD_Cmd.cdc_IOStdReq = NULL;
	}

	if(tcs_OldData -> TWFCD_Cmd.cdc_MsgPort )
	{
		DeleteMsgPort  (tcs_OldData -> TWFCD_Cmd.cdc_MsgPort);
		tcs_OldData -> TWFCD_Cmd.cdc_MsgPort = NULL;
	}

	#ifdef __PPC__

	if (tcs_OldData->TWFCD_Cmd.cdc_SCSIData)
		FreeVecPPC(tcs_OldData->TWFCD_Cmd.cdc_SCSIData);

	tcs_OldData->TWFCD_Cmd.cdc_SCSIData = 0;

	if (tcs_OldData->TWFCD_Cmd.cdc_SCSISense)
		FreeVecPPC(tcs_OldData->TWFCD_Cmd.cdc_SCSISense);

	tcs_OldData->TWFCD_Cmd.cdc_SCSISense = 0;

	if (tcs_OldData->TWFCD_Cmd.cdc_TOCBuffer)
		FreeVecPPC(tcs_OldData->TWFCD_Cmd.cdc_TOCBuffer);

	tcs_OldData->TWFCD_Cmd.cdc_TOCBuffer = 0;

	if (tcs_OldData)
		FreeVecPPC(tcs_OldData);

	#else

	if (tcs_OldData->TWFCD_Cmd.cdc_SCSIData)
		FreeVec(tcs_OldData->TWFCD_Cmd.cdc_SCSIData);

	tcs_OldData->TWFCD_Cmd.cdc_SCSIData = 0;

	if (tcs_OldData->TWFCD_Cmd.cdc_SCSISense)
		FreeVec(tcs_OldData->TWFCD_Cmd.cdc_SCSISense);

	tcs_OldData->TWFCD_Cmd.cdc_SCSISense = 0;

	if (tcs_OldData->TWFCD_Cmd.cdc_TOCBuffer)
		FreeVec(tcs_OldData->TWFCD_Cmd.cdc_TOCBuffer);

	tcs_OldData->TWFCD_Cmd.cdc_TOCBuffer = 0;

	if (tcs_OldData)
		FreeVec(tcs_OldData);

	#endif

	tcs_OldData=0;
	return(TWFCD_OK);

} /*GFE*/


/* ULONG TWFCD_ReadTOC(void)						     */
/* -=-=-=-=-=-=-=-=-=-=-=-=-						     */
/* This routine loads the table of contents off a CD and calculates a unique */
/* disc identifier. out of range track identifiers are ignored (except by the*/
/* ID generation) as most disks seem to have a mysterious track 170..?!!     */

/*GFS*/ ULONG TWFCD_ReadTOC(REG(a0) struct TWFCDData *tcrt_CDData)
{
	ULONG		ReturnCode = 0;
	BYTE		Position   = 0;
	ULONG		Status	   = 0;
	ULONG		TOCLength  = 0;
	ULONG		TOC170Adr  = 0;	    // This records the address of track 170
	BOOL		Enhanced   = FALSE; // TRUE if a data track above track 1.
	struct TOCData *TOCPtr	   = NULL;
	struct TOCData *NextTOCPtr = NULL;

	ULONG LastStart = 0;
	ULONG ThisStart = 0;

	UBYTE ReadTOCCmd[10] = {
		SCSI_CMD_READTOC,
		0,
		PAD,
		PAD,
		PAD,
		PAD,
		0,		// Starting track
		0x03, 0x24,	// Max. TOC data on current CDs is 0x324 bytes
				// or 100 track descriptors
		PAD
	};

	// Check for CD in drive
	tcrt_CDData -> TWFCD_Cmd.cdc_IOStdReq->io_Command = TD_CHANGESTATE;
	DoIO((struct IORequest *)tcrt_CDData -> TWFCD_Cmd.cdc_IOStdReq);

	if(tcrt_CDData -> TWFCD_Cmd.cdc_IOStdReq->io_Actual != 0)
	{
		tcrt_CDData -> TWFCD_Table.cda_FirstTrack  = 0;
		tcrt_CDData -> TWFCD_Table.cda_LastTrack   = 0;
		tcrt_CDData -> TWFCD_Table.cda_FirstAudio  = 0;
		tcrt_CDData -> TWFCD_Table.cda_CDLength	   = 0;
		tcrt_CDData -> TWFCD_Table.cda_AudioLen	   = 0;
		tcrt_CDData -> TWFCD_Table.cda_DiskId[0]   = '\0';
		tcrt_CDData -> TWFCD_Track.cds_AudioStatus = TWFCD_AUDIOSTATUS_INVALID;
		tcrt_CDData -> TWFCD_Track.cds_PlayTrack   = 0;
		return(TWFCD_FAIL);
	}

	// Setup the direct SCSICmd
	Status = TWFCD_DoSCSICmd(tcrt_CDData, tcrt_CDData -> TWFCD_Cmd.cdc_TOCBuffer, CDTOC_SIZE,
		ReadTOCCmd, sizeof(ReadTOCCmd), (SCSIF_READ | SCSIF_AUTOSENSE));

	if(Status != TWFCD_OK)
	{
		DEBUGLOG(kprintf("TWFCD_ReadTOC(): Status is not OK\n");)
		return(TWFCD_FAIL);
	}

	// Read the TOC in!
	ReturnCode = DoIO((struct IORequest *)tcrt_CDData -> TWFCD_Cmd.cdc_IOStdReq);

	// Check the status of the direct cmd
	if(tcrt_CDData -> TWFCD_Cmd.cdc_SCSICmd.scsi_SenseActual != 0)
	{
		DEBUGLOG(kprintf("TWFCD_ReadTOC(): Command failed\n");)

		if(tcrt_CDData -> TWFCD_Table.cda_DiskId[0] != '\0')
		{
			tcrt_CDData -> TWFCD_Table.cda_FirstTrack  = 0;
			tcrt_CDData -> TWFCD_Table.cda_LastTrack   = 0;
			tcrt_CDData -> TWFCD_Table.cda_FirstAudio  = 0;
			tcrt_CDData -> TWFCD_Table.cda_CDLength	   = 0;
			tcrt_CDData -> TWFCD_Table.cda_AudioLen	   = 0;
			tcrt_CDData -> TWFCD_Table.cda_DiskId[0]   = '\0';
			tcrt_CDData -> TWFCD_Track.cds_AudioStatus = TWFCD_AUDIOSTATUS_INVALID;
			tcrt_CDData -> TWFCD_Track.cds_PlayTrack   = 0;
		}

		return(TWFCD_FAIL);
	}

	// Read the length of the TOC - encoded in first word
	TOCLength = (tcrt_CDData -> TWFCD_Cmd.cdc_TOCBuffer[0] << 8) | tcrt_CDData -> TWFCD_Cmd.cdc_TOCBuffer[1];

	// Set track info...
	tcrt_CDData -> TWFCD_Table.cda_FirstTrack = tcrt_CDData -> TWFCD_Cmd.cdc_TOCBuffer[2];
	tcrt_CDData -> TWFCD_Table.cda_LastTrack  = tcrt_CDData -> TWFCD_Cmd.cdc_TOCBuffer[3];

	tcrt_CDData -> TWFCD_Table.cda_CDLength = 0;
	tcrt_CDData -> TWFCD_Table.cda_AudioLen = 0;

	// Init [0] element to a safe state
	tcrt_CDData -> TWFCD_Table.cda_TrackData[0].cdt_Audio = FALSE;

	// Init first audio to TWFCD_NOAUDIO
	tcrt_CDData -> TWFCD_Table.cda_FirstAudio = TWFCD_NOAUDIO;

	// Account for First and last track numbers..
	if(TOCLength >= 2)
		TOCLength -= 2;

	for(TOCPtr = (struct TOCData *)&tcrt_CDData -> TWFCD_Cmd.cdc_TOCBuffer[4];
		(UBYTE *)TOCPtr < (&tcrt_CDData -> TWFCD_Cmd.cdc_TOCBuffer[4] + TOCLength);
		TOCPtr = (struct TOCData *)((UBYTE *)TOCPtr + 8))
	{
		NextTOCPtr = (struct TOCData *)((UBYTE *)TOCPtr + 8);

		// Check if track is audio or data
		if((TOCPtr -> Track >= tcrt_CDData -> TWFCD_Table.cda_FirstTrack) &&
			(TOCPtr -> Track <= tcrt_CDData -> TWFCD_Table.cda_LastTrack))
		{
			if(TOCPtr -> Flags & 0x04)
			{
				// Data
				tcrt_CDData -> TWFCD_Table.cda_TrackData[TOCPtr -> Track].cdt_Audio = FALSE;

				if(TOCPtr -> Track > 1)
					Enhanced = TRUE;
			}

			else
			{
				// Audio track...
				tcrt_CDData -> TWFCD_Table.cda_TrackData[TOCPtr -> Track].cdt_Audio = TRUE;

				if(TOCPtr -> Track < tcrt_CDData -> TWFCD_Table.cda_FirstAudio)
				{
					tcrt_CDData -> TWFCD_Table.cda_FirstAudio = TOCPtr -> Track;
				}

				tcrt_CDData -> TWFCD_Table.cda_AudioLen += tcrt_CDData -> TWFCD_Table.cda_TrackData[TOCPtr -> Track].cdt_Length;
			}

			tcrt_CDData -> TWFCD_Table.cda_TrackData[TOCPtr -> Track].cdt_Address = TOCPtr -> Addr;
			tcrt_CDData -> TWFCD_Table.cda_TrackData[TOCPtr -> Track].cdt_Length  = ((NextTOCPtr -> Addr - 1) - TOCPtr -> Addr); // / 75;
			tcrt_CDData -> TWFCD_Table.cda_CDLength += tcrt_CDData -> TWFCD_Table.cda_TrackData[TOCPtr -> Track].cdt_Length;
		}

		if(TOCPtr -> Track == 170)
		{
			TOC170Adr = (ULONG)TOCPtr -> Addr;
		}

		DEBUGLOG(kprintf("TWFCD_ReadTOC: TOCPtr -> track = %ld, TOCPtr -> addr = %06lX\n", (ULONG)TOCPtr -> Track, (ULONG)TOCPtr -> Addr);)
	}

	// Compose a MCDPlayer compatible CDID code.
	if(tcrt_CDData -> TWFCD_Table.cda_LastTrack > 3)
	{
		if(!Enhanced)
		{
			sprintf(tcrt_CDData -> TWFCD_Table.cda_DiskId, "ID%02lu%06lX%06lX", (ULONG)(tcrt_CDData -> TWFCD_Cmd.cdc_TOCBuffer[3] - tcrt_CDData -> TWFCD_Cmd.cdc_TOCBuffer[2]) + 1, (ULONG)tcrt_CDData -> TWFCD_Table.cda_TrackData[3].cdt_Address, TOC170Adr);
		}

		else
		{
			sprintf(tcrt_CDData -> TWFCD_Table.cda_DiskId, "ID%02lu%06lX%06lX", (ULONG)(tcrt_CDData -> TWFCD_Cmd.cdc_TOCBuffer[3] - tcrt_CDData -> TWFCD_Cmd.cdc_TOCBuffer[2]) + 1, (ULONG)tcrt_CDData -> TWFCD_Table.cda_TrackData[2].cdt_Address, TOC170Adr);
		}

	}

	else
	{
		sprintf(tcrt_CDData -> TWFCD_Table.cda_DiskId, "ID%02lu000000%06lX", (ULONG)(tcrt_CDData -> TWFCD_Cmd.cdc_TOCBuffer[3] - tcrt_CDData -> TWFCD_Cmd.cdc_TOCBuffer[2]) + 1, TOC170Adr);
	}

	DEBUGLOG(kprintf("TWFCD_ReadTOC: ID is %s\n", tcrt_CDData -> TWFCD_Table.cda_DiskId););

	tcrt_CDData -> TWFCD_Table.cda_GotTOC = TRUE;

	// Check the status of the command
	if (ReturnCode == 0)
	{
		return(TWFCD_OK);
	}

	else
	{
		return(TWFCD_FAIL);
	}

}/*GFE*/


/* ULONG TWFCD_PlayTracks(TWFCDData *, TagItem *)			     */
/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=			     */
/* This function starts the playback of one or more tracks on the CD referred*/
/* to by the TWFCDData structure. It supports several playback command forms */
/* specifically PLAYAUDIO10, PLAYAUDIO12 and PLAYAUDIO_TRACKINDEX. See the   */
/* tags for more details of this. This automatically calls TWFCD_ReadTOC if  */
/* the table of contents has not already been read.			     */

/*GFS*/ ULONG TWFCD_PlayTracks(REG(a0) struct TWFCDData *tcpt_CDData, REG(a1) struct TagItem *pt_Tags)
{
	#if defined(__VBCC__) // Cowcat

	struct TagItem *CountItem  = NULL;

	LONG	 Start	    = 0;
	LONG	 tcpt_Length = 0;
	ULONG	 ReturnCode = 0;
	ULONG	 Status	    = 0;
	ULONG	 CmdLength  = 10;

	UBYTE PlayCmd[12] =
	{
		SCSI_CMD_PLAYAUDIO_TRACKINDEX,
		PAD,
		PAD,
		PAD,
		0,   // Start track
		1,   // Index
		PAD,
		0,   // End track
		1,   // Index
		PAD,
		PAD,
		PAD
	};

	// Get the play mode
	PlayCmd[0] = (UBYTE)GetTagData(TWFCD_Track_PlayMode, SCSI_CMD_PLAYAUDIO12, pt_Tags);

	// is the TOC valid?
	if(!tcpt_CDData -> TWFCD_Table.cda_GotTOC)
	{
		ReturnCode = TWFCD_ReadTOC(tcpt_CDData);

		if(ReturnCode != TWFCD_OK)
			return(ReturnCode);
	}

	// Bomb if nothing to play.
	if(tcpt_CDData -> TWFCD_Table.cda_FirstAudio == TWFCD_NOAUDIO)
		return(TWFCD_NOAUDIO);

	// Start Track
	Start = GetTagData(TWFCD_Track_Start, tcpt_CDData -> TWFCD_Table.cda_FirstAudio, pt_Tags);

	// End Track
	if(CountItem = FindTagItem(TWFCD_Track_Count, pt_Tags))
	{
		tcpt_Length = Start + (CountItem -> ti_Data - 1);
	}

	else
	{
		tcpt_Length = GetTagData(TWFCD_Track_End, tcpt_CDData -> TWFCD_Table.cda_LastTrack, pt_Tags);
	}

	// Make sure Start < End
	if(Start > tcpt_Length)
	{
		ReturnCode = Start;
		Start = tcpt_Length;
		tcpt_Length = ReturnCode;
	}

	// sort out the command...
	if(PlayCmd[0] == SCSI_CMD_PLAYAUDIO_TRACKINDEX)
	{
		PlayCmd[4] = Start;
		PlayCmd[7] = tcpt_Length;
	}

	else
	{
		// Common to PLAY10 and PLAY12
		Start = tcpt_CDData -> TWFCD_Table.cda_TrackData[Start].cdt_Address;
		PlayCmd[2] = (Start & 0xFF000000) >> 24;
		PlayCmd[3] = (Start & 0x00FF0000) >> 16;
		PlayCmd[4] = (Start & 0x0000FF00) >>  8;
		PlayCmd[5] = (Start & 0x000000FF);

		tcpt_Length = (tcpt_CDData -> TWFCD_Table.cda_TrackData[tcpt_Length].cdt_Address - Start) + 
			tcpt_CDData -> TWFCD_Table.cda_TrackData[tcpt_Length].cdt_Length;

		if(PlayCmd[0] == SCSI_CMD_PLAYAUDIO12)
		{
			PlayCmd[6] = (tcpt_Length & 0xFF000000) >> 24;
			PlayCmd[7] = (tcpt_Length & 0x00FF0000) >> 16;
			PlayCmd[8] = (tcpt_Length & 0x0000FF00) >>  8;
			PlayCmd[9] = (tcpt_Length & 0x000000FF);

			CmdLength = 12;
		}

		else
		{
			// PLAY10
			PlayCmd[7] = (tcpt_Length & 0x0000FF00) >> 8;
			PlayCmd[8] = (tcpt_Length & 0x000000FF);
		}
	}

	// Setup the direct SCSICmd
	Status = TWFCD_DoSCSICmd(tcpt_CDData, tcpt_CDData -> TWFCD_Cmd.cdc_SCSIData, CDBUFFER_SIZE,
		PlayCmd, CmdLength, (SCSIF_READ | SCSIF_AUTOSENSE));

	if(Status != TWFCD_OK)
		return(TWFCD_FAIL);

	// Play the tracks - asynchronously (the play returns
	// right after a successful start)
	ReturnCode = DoIO((struct IORequest *) tcpt_CDData -> TWFCD_Cmd.cdc_IOStdReq);

	// Check the status of the direct cmd
	if (tcpt_CDData -> TWFCD_Cmd.cdc_SCSICmd.scsi_SenseActual != 0)
	{
		return(TWFCD_FAIL);
	}

	// Check the status of the command
	if(ReturnCode == 0)
	{
		return(TWFCD_OK);
	}

	else
	{
		return(TWFCD_FAIL);
	}

	#endif

} /*GFE*/


/* ULONG TWFCD_StopPlay(TWFCDData *)					     */
/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-					     */
/* This halts playback on the CD identified by the CD data structure. Note   */
/* that some CD players may not function correctly with this version, I am   */
/* researching other methods of playback halting.			     */

/*GFS*/ ULONG TWFCD_StopPlay(REG(a0) struct TWFCDData *tcsp_CDData)
{
	ULONG ReturnCode = 0;
	ULONG Status	 = 0;

	UBYTE StopCmd[6] =
	{
		SCSI_CMD_START_STOP_UNIT,
		PAD,
		PAD,
		PAD,
		PAD,
		PAD
	};

	DEBUGLOG(kprintf("TWFCD_StopPlay: in stop, compiling command\n");)
 
	// Check for CD in drive
	tcsp_CDData -> TWFCD_Cmd.cdc_IOStdReq->io_Command = TD_CHANGESTATE;
	DoIO((struct IORequest *)tcsp_CDData -> TWFCD_Cmd.cdc_IOStdReq);

	if(tcsp_CDData -> TWFCD_Cmd.cdc_IOStdReq->io_Actual != 0)
		return(TWFCD_OK);

	// Setup the direct SCSICmd
	Status = TWFCD_DoSCSICmd(tcsp_CDData, tcsp_CDData -> TWFCD_Cmd.cdc_SCSIData, CDBUFFER_SIZE,
		StopCmd, sizeof(StopCmd), (SCSIF_READ | SCSIF_AUTOSENSE));

	DEBUGLOG(kprintf("TWFCD_StopPlay: command ready\n");)

	if(Status != TWFCD_OK)
		return(TWFCD_FAIL);

	DEBUGLOG(kprintf("TWFCD_StopPlay: Sending command to device\n");)

	// Stop the CD (a play of length 0)
	ReturnCode = DoIO((struct IORequest *)tcsp_CDData -> TWFCD_Cmd.cdc_IOStdReq);

	DEBUGLOG(kprintf("TWFCD_StopPlay: command sent, checking sense data\n");)

	// Check the status of the direct cmd
	if(tcsp_CDData -> TWFCD_Cmd.cdc_SCSICmd.scsi_SenseActual != 0)
		return(TWFCD_FAIL);

	DEBUGLOG(kprintf("TWFCD_StopPlay: sense check passed ok, returning\n");)

	// Check the status of the command
	if(ReturnCode == 0)
	{
		// not paused any more!
		tcsp_CDData -> TWFCD_Table.cda_Paused = FALSE;
		return(TWFCD_OK);
	}

	else
	{
		return(TWFCD_FAIL);
	}

} /*GFE*/


/* ULONG TWFCD_PausePlay(TWFCDData *)					     */
/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=					     */
/* As you'd expect, this pauses the playback on the specified device. Call   */
/* this again to resume the playback.					     */

/*GFS*/ ULONG TWFCD_PausePlay(REG(a0) struct TWFCDData *tcpp_CDData)
{
	ULONG ReturnCode = 0;
	ULONG Status	 = 0;

	UBYTE PauseCmd[10] = {
		SCSI_CMD_PAUSE_RESUME,
		PAD,
		PAD,
		PAD,
		PAD,
		PAD,
		PAD,
		PAD,
		0,
		PAD
	};

	if(tcpp_CDData -> TWFCD_Table.cda_Paused)
	{
		DEBUGLOG(kprintf("TWFCD_PausePlay: CD is paused, resuming playback\n");)
		PauseCmd[8] = 1;
		tcpp_CDData -> TWFCD_Table.cda_Paused = FALSE;
	}

	else
	{
		DEBUGLOG(kprintf("TWFCD_PausePlay: CD is playing, pausing playback\n");)
		PauseCmd[8] = 0;
		tcpp_CDData -> TWFCD_Table.cda_Paused = TRUE;
	}

	// Setup the direct SCSICmd
	Status = TWFCD_DoSCSICmd(tcpp_CDData, tcpp_CDData -> TWFCD_Cmd.cdc_SCSIData, CDBUFFER_SIZE,
		PauseCmd, sizeof(PauseCmd), (SCSIF_READ | SCSIF_AUTOSENSE));

	if(Status != TWFCD_OK)
		return(TWFCD_FAIL);

	// Send the command...
	ReturnCode = DoIO((struct IORequest *)tcpp_CDData -> TWFCD_Cmd.cdc_IOStdReq);

	// Check the status of the direct cmd
	if (tcpp_CDData -> TWFCD_Cmd.cdc_SCSICmd.scsi_SenseActual != 0)
		return(TWFCD_FAIL);

	DEBUGLOG(kprintf("TWFCD_PausePlay: return code is %d\n", ReturnCode);)

	// Check the status of the command
	if (ReturnCode == 0)
	{
		return(TWFCD_OK);
	}

	else
	{
		return(TWFCD_FAIL);
	}

} /*GFE*/


/* ULONG TWFCD_MotorControl(TWFCDData *, UBYTE)				     */
/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=				     */
/* This allows the motor and tray of the CD to be controlled. The value in   */
/* newStatus determines the operation you wish to perform, valid parameters  */
/* are:									     */
/*									     */
/* TWFCD_MOTOR_STOP	- Turn off the drive (stops playback automagically)  */
/* TWFCD_MOTOR_START	- Turns the motor on (but does nothing else..)	     */
/* TWFCD_MOTOR_EJECT	- Opens the CD tray if it is not already open.	     */
/* TWFCD_MOTOR_LOAD	- Closes the CD tray.				     */

/*GFS*/ ULONG TWFCD_MotorControl(REG(a0) struct TWFCDData *tcmc_CDData, REG(d0) UBYTE newStatus)
{
	ULONG returnCode  = 0;
	ULONG status	  = 0;
	UBYTE ejectCmd[6] =
	{
		SCSI_CMD_START_STOP_UNIT,
		0,
		PAD,
		PAD,
		0,	// Byte 5 = start/stop eject/insert
		PAD,
	};

	ejectCmd[4] = newStatus;
	status = TWFCD_DoSCSICmd(tcmc_CDData, tcmc_CDData -> TWFCD_Cmd.cdc_SCSIData, CDBUFFER_SIZE,
		&ejectCmd[0], sizeof(ejectCmd), (SCSIF_READ | SCSIF_AUTOSENSE));

	if(status != TWFCD_OK)
	{
		return(TWFCD_FAIL);
	}

	returnCode = DoIO((struct IORequest *) tcmc_CDData -> TWFCD_Cmd.cdc_IOStdReq);

	// Check the status of the direct cmd
	if (tcmc_CDData -> TWFCD_Cmd.cdc_SCSICmd.scsi_SenseActual != 0)
	{
		// Conceivably, someone may want to know why, so a debug
		// call should interpret the field
		return(TWFCD_FAIL);
	}

	// Check the status of the command
	if (returnCode == 0)
	{
		return(TWFCD_OK);
	}

	else
	{
		return(TWFCD_FAIL);
	}

}/*GFE*/


/* ULONG TWFCD_ReadSubChannel(TWFCDData *)				     */
/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-				     */
/* This reads the sub-Q channel data from the specified CD. It only reads in */
/* format 1 (CD position information), although it could be extended to	     */
/* support all format modes.						     */

/*GFS*/ ULONG TWFCD_ReadSubChannel(REG(a0) struct TWFCDData *tcrs_CDData)
{
	struct SubQData *chanData = NULL;
	ULONG returnCode = 0;
	ULONG status	 = 0;

	UBYTE readCmd[10] =
	{
		SCSI_CMD_READSUB_CHANNEL,
		0,
		0x40,
		0x01,
		0,
		0,
		0,
		0,
		CDBUFFER_SIZE,
		0,
	};

	// Setup the direct SCSICmd
	status = TWFCD_DoSCSICmd(tcrs_CDData, tcrs_CDData -> TWFCD_Cmd.cdc_SCSIData, CDBUFFER_SIZE,
		&readCmd[0], sizeof(readCmd), (SCSIF_READ | SCSIF_AUTOSENSE));

	if (status != TWFCD_OK)
	{
		return (TWFCD_FAIL);
	}

	returnCode = DoIO((struct IORequest *) tcrs_CDData -> TWFCD_Cmd.cdc_IOStdReq);

	// Check the status of the direct cmd
	if(tcrs_CDData -> TWFCD_Cmd.cdc_SCSICmd.scsi_SenseActual != 0)
	{
		// Conceivably, someone may want to know why, so a debug
		// call should interpret the field
		return(TWFCD_FAIL);
	}

	chanData = (struct SubQData *)tcrs_CDData -> TWFCD_Cmd.cdc_SCSIData;

	// Copy the settings...
	tcrs_CDData -> TWFCD_Track.cds_AudioStatus = chanData -> AudioStatus;

	// ... provided they are valid of course...
	if((chanData -> AudioStatus != TWFCD_AUDIOSTATUS_INVALID) &&
		(chanData -> AudioStatus != TWFCD_AUDIOSTATUS_NOSTATUS))
	{

		tcrs_CDData -> TWFCD_Track.cds_PlayTrack   = chanData -> TrackNum   ;
		tcrs_CDData -> TWFCD_Track.cds_RelAddress  = chanData -> RelativeAdr;

		if(tcrs_CDData -> TWFCD_Table.cda_GotTOC)
		{
			tcrs_CDData -> TWFCD_Track.cds_RelRemain = 
				tcrs_CDData -> TWFCD_Table.cda_TrackData[chanData -> TrackNum].cdt_Length - chanData -> RelativeAdr;
		} 

		else
		{
			tcrs_CDData -> TWFCD_Track.cds_RelRemain = 0;
		}

		tcrs_CDData -> TWFCD_Track.cds_AbsAddress  = chanData -> AbsoluteAdr;

		if(tcrs_CDData -> TWFCD_Table.cda_GotTOC)
		{
			tcrs_CDData -> TWFCD_Track.cds_AbsRemain = tcrs_CDData -> TWFCD_Table.cda_CDLength - chanData -> AbsoluteAdr;
		}

		else
		{
			tcrs_CDData -> TWFCD_Track.cds_AbsRemain = 0;
		}
	}

	// Check the status of the command
	if (returnCode == 0)
	{
		return(TWFCD_OK);
	}

	else
	{
		return(TWFCD_FAIL);
	}

} /*GFE*/


/* ULONG TWFCD_doSCSICmd(TWFCDData *, UBYTE *, int, UBYTE *, int, UBYTE)     */
/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-     */
/* This compiles the scsi-direct command ready for sending to the device via */
/* DoIO(). It ensures the SCSI command structure in the TWFCDData struct is  */
/* set up correctly and the IORequest contains the command.		     */

/*GFS*/ static ULONG TWFCD_DoSCSICmd(struct TWFCDData *tcdc_CDData, UBYTE *buffer, int bufferSize, UBYTE *cmd, int cmdSize, UBYTE flags)
{
	// Set up the Play IOReq
	tcdc_CDData -> TWFCD_Cmd.cdc_IOStdReq->io_Command = HD_SCSICMD;
	tcdc_CDData -> TWFCD_Cmd.cdc_IOStdReq->io_Length = sizeof(struct SCSICmd);
	tcdc_CDData -> TWFCD_Cmd.cdc_IOStdReq->io_Data = (APTR)&tcdc_CDData -> TWFCD_Cmd.cdc_SCSICmd;

	// Setup the direct SCSICmd
	tcdc_CDData -> TWFCD_Cmd.cdc_SCSICmd.scsi_Data = (APTR) buffer;
	tcdc_CDData -> TWFCD_Cmd.cdc_SCSICmd.scsi_Length = bufferSize;

	tcdc_CDData -> TWFCD_Cmd.cdc_SCSICmd.scsi_SenseActual = 0;
	tcdc_CDData -> TWFCD_Cmd.cdc_SCSICmd.scsi_SenseData = tcdc_CDData -> TWFCD_Cmd.cdc_SCSISense;
	tcdc_CDData -> TWFCD_Cmd.cdc_SCSICmd.scsi_SenseLength = CDSENSE_SIZE;

	tcdc_CDData -> TWFCD_Cmd.cdc_SCSICmd.scsi_Command = (UBYTE *) cmd;
	tcdc_CDData -> TWFCD_Cmd.cdc_SCSICmd.scsi_CmdLength = cmdSize;
	tcdc_CDData -> TWFCD_Cmd.cdc_SCSICmd.scsi_Flags = flags;

	return (TWFCD_OK);

}/*GFE*/



