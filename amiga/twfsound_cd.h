//     ___       ___
//   _/  /_______\  \_     ___ ___ __ _                       _ __ ___ ___
//__//  / _______ \  \\___/                                               \___
//_/ | '  \__ __/  ` | \_/        © Copyright 1998, Christopher Page       \__
// \ | |    | |__  | | / \               All Rights Reserved               /
//  >| .    |  _/  . |<   >--- --- -- -                       - -- --- ---<
// / \  \   | |   /  / \ / This file may not be distributed, reproduced or \
// \  \  \_/   \_/  /  / \  altered, in full or in part, without written   /
//  \  \           /  /   \    permission from Christopher Page. Legal    /
// //\  \_________/  /\\ //\     action will be taken in cases where     /
//¯ ¯¯\   _______   /¯¯ ¯ ¯¯\        this notice is not obeyed.         /¯¯¯¯¯
//¯¯¯¯¯\_/       \_/¯¯¯¯¯¯¯¯¯\   ___________________________________   /¯¯¯¯¯¯
//                            \_/                                   \_/
//
// Description:
//
//  Sound library header (development version).
//
// Detail:
//
//  This file contains the structyres, defines and includes used by the
//  TWFSound library and it's routines.
//
// Vesion:
//
//  $VER: twfsound_cd.h 3.12 (17/10/1999)
//
// Fold Markers:
//
//  Start: /*GFS*/
//    End: /*GFE*/


#include<exec/exec.h>
#include<dos/dos.h>
#include<dos/dostags.h>
#include<dos/dosextens.h>
#include<devices/scsidisk.h>
#include<utility/utility.h>
#include<utility/tagitem.h>

//#include<clib/exec_protos.h>
//#include<clib/dos_protos.h>
#include<clib/alib_protos.h>
#include<clib/utility_protos.h>

#include <proto/exec.h>
#include <proto/dos.h>
//#include <proto/utility.h>


#define REG(x)

// General Codes
#define TWFCD_OK                       0x1000

// Error Codes
#define TWFCD_FAIL                     0x2000
#define TWFCD_FAIL_OPEN_DEVICE         0x2001
#define TWFCD_FAIL_NOT_AUDIO           0x2002
#define TWFCD_NOAUDIO                     255

// Structure sizes
#define CDBUFFER_SIZE                     252
#define CDSENSE_SIZE                      252
#define CDTOC_SIZE                        804  // Allows for 200 track descriptors
#define PAD                                 0

// SCSI-2 CD-ROM command codes
#define SCSI_CMD_READSUB_CHANNEL          0x42
#define SCSI_CMD_READTOC                  0x43
#define SCSI_CMD_PLAYAUDIO10              0x45
#define SCSI_CMD_PLAYAUDIO12              0xA5
#define SCSI_CMD_PLAYAUDIO_TRACKINDEX     0x48
#define SCSI_CMD_PAUSE_RESUME             0x4B
#define SCSI_CMD_START_STOP_UNIT          0x1B

// values which can be passed to TWFCD_MotorControl()
#define TWFCD_MOTOR_STOP                  0x00
#define TWFCD_MOTOR_START                 0x01
#define TWFCD_MOTOR_EJECT                 0x02
#define TWFCD_MOTOR_LOAD                  0x03

// Tags
// Index of the first track to play, default is first audio track
#define TWFCD_Track_Start         (TAG_USER|(128<<16)|0x0000)   // (UBYTE)

// Index of the last track to play, default is 99
#define TWFCD_Track_End           (TAG_USER|(128<<16)|0x0001)   // (UBYTE)

// Number of tracks to play (overrides TWFCD_Track_End). Default 99
#define TWFCD_Track_Count         (TAG_USER|(128<<16)|0x0002)   // (UBYTE)

// Play mode, use one of the SCSI_CMD_PLAYAUDIO#?s above. Def is PLAYAUDIO12
#define TWFCD_Track_PlayMode      (TAG_USER|(128<<16)|0x0003)   // (UBYTE)


struct TWFCD_Command
{
    struct MsgPort  *cdc_MsgPort      ;  // Used to reply to the program from the device
    struct IOStdReq *cdc_IOStdReq     ;  // Used to send cmds to the device
    struct SCSICmd   cdc_SCSICmd      ;  // Direct access to the CD through the SCSI device

    // Generic buffers in chip ram
    UBYTE *cdc_SCSIData;  // Buffer for scsi_Data field
    UBYTE *cdc_SCSISense;  // Buffer for scsi_Sense [ERRORS!]
    UBYTE *cdc_TOCBuffer;  // Stores the table of contents for the CD-ROM disc
};

struct TWFCD_Track
{
    ULONG cdt_Address; // Logical address on the CD
    BOOL  cdt_Audio  ; // TRUE if the track is an audio track
    ULONG cdt_Length ; // Length in logical blocks, divide by 75 to get approximate
                       // length in seconds
};

struct TWFCD_Attrs
{
           BOOL        cda_GotTOC    ;      // TRUE if TOC has been read
           BOOL        cda_Paused    ;      // Go on, have a guess.
           UBYTE       cda_FirstTrack;      // # of first real track
           UBYTE       cda_FirstAudio;      // # of first audio track
           UBYTE       cda_LastTrack ;      // maximum track number
           ULONG       cda_CDLength  ;      // Length of the CD, divide by 75 to get length in seconds
           ULONG       cda_AudioLen  ;      // Length of the audio tracks, divide by 75 for seconds
           char        cda_DiskId[20];      // MCDPlayer compatible!
    struct TWFCD_Track cda_TrackData[100];  // 1+ indexed, 0 is not used (easier to deal with 1-indexed CD drive)
};

#define TWFCD_AUDIOSTATUS_INVALID   0x00
#define TWFCD_AUDIOSTATUS_PLAYING   0x11
#define TWFCD_AUDIOSTATUS_PAUSED    0x12
#define TWFCD_AUDIOSTATUS_FINISHED  0x13
#define TWFCD_AUDIOSTATUS_ERROR     0x14
#define TWFCD_AUDIOSTATUS_NOSTATUS  0x15

struct TWFCD_SubData
{
    UBYTE  cds_AudioStatus;
    UBYTE  cds_PlayTrack  ; // not valid if audio status is INVALID or NOSTATUS
    ULONG  cds_RelAddress ; // Divide the following 4 fields by 75 to get the seconds equivalents
    ULONG  cds_RelRemain  ;
    ULONG  cds_AbsAddress ;
    ULONG  cds_AbsRemain  ;
};

struct TWFCDData
{
    struct TWFCD_Command TWFCD_Cmd;
    struct TWFCD_Attrs   TWFCD_Table;
    struct TWFCD_SubData TWFCD_Track;
};

extern struct TWFCDData * TWFCD_Setup         (STRPTR DeviceName, UBYTE UnitNum);
extern ULONG              TWFCD_Shutdown      (struct TWFCDData *tcs_OldData);
extern ULONG              TWFCD_ReadTOC       (struct TWFCDData *tcrt_CDData);
extern ULONG              TWFCD_PlayTracks    (struct TWFCDData *tcpt_CDData, struct TagItem *pt_Tags);
extern ULONG              TWFCD_StopPlay      (struct TWFCDData *tcsp_CDData);
extern ULONG              TWFCD_PausePlay     (struct TWFCDData *tcpp_CDData);
extern ULONG              TWFCD_MotorControl  (struct TWFCDData *tcet_CDData, UBYTE newStatus);
extern ULONG              TWFCD_ReadSubChannel(struct TWFCDData *tcrs_CDData);

#ifndef NDEBUG
    extern void kprintf(UBYTE *fmt,...);
//    #define DEBUGLOG(x) x
		#define DEBUGLOG(x)
#else
    #define DEBUGLOG(x)
#endif
