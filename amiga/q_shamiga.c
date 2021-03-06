/*****************************************************************
*18.9.1999 SH    - fixed for Unixlib                             *
* (Hyperion Software)                                            *
*****************************************************************/

#include "../qcommon/qcommon.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <fnmatch.h>
//#include <sys/time.h> // Cowcat

#pragma pack(push,2)

#include <utility/tagitem.h>
#include <exec/exec.h>
#include <dos/dos.h>
#include <dos/dosasl.h>
#include <proto/exec.h>
#include <proto/dos.h>

#ifdef __PPC__
#if defined(__GNUC__)
#include <powerpc/powerpc.h>
#include <powerpc/powerpc_protos.h>
#else
#include <powerpc/powerpc.h>
#include <proto/powerpc.h>
#endif

#else

#include <inline/timer_protos.h> // Cowcat

#endif

#pragma pack(pop)

//===============================================================================

#define HUNK_MAXHUNKS 50
int     hunkcount=0;

void    **membase;
int     hunkmaxsize;
int     cursize;
int     alloccount;

void *Hunk_Begin (int maxsize)
{
	hunkmaxsize = maxsize;
	membase=malloc(HUNK_MAXHUNKS*sizeof(void *));

	if (!membase)
		Sys_Error ("Hunk_Begin reserve failed");

	memset(membase,0,HUNK_MAXHUNKS*sizeof(void *));
	cursize=0L;
	alloccount=0;
	return membase;
}

void *Hunk_Alloc (int size)
{
    	// round to cacheline
    	void *mem;
    	void **tempmembase=membase;
    	int count=alloccount;
    	size = (size+31)&~31;

    	cursize += size;

    	if (cursize > hunkmaxsize)
		Sys_Error ("Hunk_Alloc overflow");

    	if(!(mem=malloc(size)))
		Sys_Error("Hunk_Alloc failed: Could not allocate %i Bytes !",size);

    	memset(mem,0,size);

    	while(count>=HUNK_MAXHUNKS)
    	{
		tempmembase=tempmembase[HUNK_MAXHUNKS-1];
		count-=HUNK_MAXHUNKS;
    	}

    	tempmembase[count]=mem;
    	alloccount++;

	if(alloccount%HUNK_MAXHUNKS==HUNK_MAXHUNKS-1)
	{
		void **newmembase;
		// fprintf(stderr,"Adding hunks!\n");
		newmembase=malloc(HUNK_MAXHUNKS*sizeof(void *));

		if (!newmembase)
			Sys_Error ("Hunk_Begin reserve failed");

		memset(newmembase,0,HUNK_MAXHUNKS*sizeof(void *));
		tempmembase[HUNK_MAXHUNKS-1]=newmembase;
		alloccount++;
	}

    	return mem;
}

int Hunk_End (void)
{
	// free the remaining unused virtual memory
    	hunkcount++;
    	return cursize;
}

void Hunk_Free (void *base)
{
	int 	i=0;
	void 	**memarray=base;

    	if ( memarray )
    	{
		while(memarray[i])
		{
			if(i==HUNK_MAXHUNKS-1)
			{
				void *t=memarray;
				i=0;
				memarray=memarray[HUNK_MAXHUNKS-1];
				free(t);
				continue;
			}

			free(memarray[i++]);
		}

		free (memarray);
    	}

    	hunkcount--;
}

//===============================================================================


// gettimeofday pasted here from posix.lib Cowcat

#if 0

#ifndef TIMERNAME

struct timeval {
	long tv_sec;
	long tv_usec;
};

#else

#define tv_sec tv_secs
#define tv_usec tv_micro

#endif

#define UNIXOFFSET 252460800  /* seconds from 1.1.70 to 1.1.78 */

#ifndef __PPC__

int gettimeofday (struct timeval *tv)
{
  	struct DateStamp ds;

  	DateStamp(&ds);
  	tv->tv_sec = (long)(UNIXOFFSET + ds.ds_Days*(60*60*24) + ds.ds_Minute*60 + ds.ds_Tick/TICKS_PER_SECOND);  		
  	tv->tv_usec = (long)(ds.ds_Tick%TICKS_PER_SECOND)*(1000000/TICKS_PER_SECOND);
  	return 0;
}

#else

int gettimeofday (struct timeval *tv)
{
  	struct timeval t;
   
  	static struct Device *TimerBase;

	#ifdef __PPC__

  	GetSysTimePPC(&t);

	#else

  	if (!TimerBase)
		TimerBase=(struct Device *)FindName(&SysBase->DeviceList,"timer.device");

 	GetSysTime(&t);

	#endif

  	tv->tv_sec = t.tv_secs;
  	tv->tv_usec = t.tv_micro;	
  
  	return 0;
}

#endif

unsigned int inittime=0L;

unsigned int timeGetTime(void)
{
  	struct timeval 	tv;
  	unsigned int 	currenttime;

  	gettimeofday(&tv);
  
  	#ifndef __PPC__

  	currenttime=tv.tv_sec - UNIXOFFSET;  // tv.tv_sec-2922*1440*60

  	#else

  	currenttime = tv.tv_sec;

  	#endif

  	if(inittime == 0L)
		inittime = currenttime;

  	currenttime = currenttime-inittime;

  	return currenttime * 1000 + tv.tv_usec / 1000;
}

#else // new Cowcat

unsigned int inittime = 0L;

unsigned int timeGetTime(void)
{
	struct timeval 	tv;
  	unsigned int 	currenttime;
	
  	#ifdef __PPC__

  	GetSysTimePPC(&tv);

	#else
	
	static struct Device *TimerBase;

	if (!TimerBase)
		TimerBase = (struct Device *)FindName(&SysBase->DeviceList,"timer.device");

 	GetSysTime(&tv);

	#endif

  	currenttime = tv.tv_secs;

	if (!inittime)
		inittime = currenttime;

  	currenttime = currenttime-inittime;

  	return currenttime * 1000 + tv.tv_micro / 1000;
}

#endif

/*
================
Sys_Milliseconds
================
*/

int	curtime;

int Sys_Milliseconds (void)
{
	curtime = timeGetTime();
	return curtime;
}

void Sys_Mkdir (char *path)
{
    	int i = mkdir (path,0755);

    	if (i == -1 && errno != EEXIST && errno != 26)
    	{
		#ifdef __PPC__

		printf("Error creating '%s':", path);

		switch(errno)
		{
	    		case ENOTDIR:
				printf("A component of the path prefix is not a directory\n");
				break;

	    		case ENOENT:
				printf("A component of the path prefix does not exist\n");
				break;

	    		case EACCES:
				printf("Access denied\n");
				break;

	    		case EROFS:
				printf("Read-only file system\n");
				break;

	    		default:
				printf("Errno = %d\n", errno);
				break;
		}

		#endif
    	}
}

//============================================

char    findbase[MAX_OSPATH];
char    findmask[MAX_OSPATH];
char    findpath[MAX_OSPATH];
DIR *   findhandle=NULL;

static qboolean CompareAttributes( unsigned found, unsigned musthave, unsigned canthave )
{
	return true;
}

#define MAX_DRIVE  20
#define MAX_DIR    255
#define MAX_FNAME  255
#define MAX_EXT    20

typedef struct {
    	char    name[MAX_FNAME+MAX_EXT+1];
    	int     attrib;
    	long    size;
    	long    time_write;
    	// ...

} _finddata_t;

char nameBuffer[512];

void stormPathConvert(const char *path, char *output)
{
    	while (*path)
	{
		if (*path == '\\')
			*output = '/';

		else
			*output = *path;

		path++; output++;
    	}

    	*output = '\0';
}

_finddata_t fileinfo1;
long myfindhandle=0;

void FillInfo(struct AnchorPath *ap, _finddata_t *fileinfo)
{
    	strncpy(fileinfo->name, ap->ap_Buf, 250);

    	if (ap->ap_Info.fib_EntryType >= 0)
		fileinfo->attrib = 1;

    	else
		fileinfo->attrib = 0;

    	fileinfo->size = ap->ap_Info.fib_Size;

    	fileinfo->time_write = ap->ap_Info.fib_Date.ds_Days*4320000
		+ ap->ap_Info.fib_Date.ds_Minute*3000
		+ ap->ap_Info.fib_Date.ds_Tick;
}


long _findfirst(char *path, _finddata_t *fileinfo)
{
    	#ifdef __PPC__ // Cowcat

    	struct AnchorPath *ap = (struct AnchorPath *)AllocVecPPC(sizeof(struct AnchorPath)+256, MEMF_CLEAR,0);

    	#else

    	struct AnchorPath *ap = (struct AnchorPath *)AllocVec(sizeof(struct AnchorPath)+256, MEMF_CLEAR);

    	#endif

    	if (!ap)
		return 0L;

    	stormPathConvert(path, nameBuffer);

    	ap->ap_BreakBits = 0;  // was NULL Cowcat
    	ap->ap_Strlen = 250;
    	ap->ap_Flags  = APF_DODOT;

    	if (MatchFirst((STRPTR)nameBuffer, ap) != 0)
	{
		#ifdef __PPC__
		FreeVecPPC(ap);
		#else 
		FreeVec(ap);
		#endif
	
		return 0;
    	}
	
	else
	{
		FillInfo(ap, fileinfo);
    	}

    	return (long)ap;
}

long _findnext(long FindHandle, _finddata_t *fileinfo)
{
    	if (0 !=  MatchNext((struct AnchorPath *)FindHandle))
		return -1;

    	FillInfo((struct AnchorPath *)FindHandle, fileinfo);
    	return 0;
}

void _findclose(long FindHandle)
{
    	if (FindHandle)
	{
		MatchEnd((struct AnchorPath *)FindHandle);

		#ifdef __PPC__
		FreeVecPPC((void *)FindHandle);
		#else
		FreeVec((void *)FindHandle);
		#endif
    	}
}

char *Sys_FindFirst (char *path, unsigned musthave, unsigned canthave )
{
  	strcpy(fileinfo1.name,"");
  	myfindhandle=_findfirst(path,&fileinfo1);

  	if (strlen(fileinfo1.name)<1)
		return 0;

  	else
		return fileinfo1.name;
}
    
char *Sys_FindNext ( unsigned musthave, unsigned canthave )
{
    	if (_findnext(myfindhandle,&fileinfo1)==-1)
		return 0;

     	else
		return fileinfo1.name;
}

void Sys_FindClose (void)
{
    	if (myfindhandle)
		_findclose(myfindhandle);

    	myfindhandle=0;
}


//============================================

