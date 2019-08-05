/*****************************************************************
* 21-Dec-1999 PA -    Generic LRU-Caching                        *
*                     Initial version                            *
* (Hyperion Software)                                            *
*****************************************************************/

#include <stdlib.h>
#include <stdio.h>
#pragma amiga-align
#include <proto/dos.h>
#pragma default-align

#include "lrucache.h"

void LC_FlushOldestMember(struct lc_cache *cache);
static char filename[32];

struct lc_cache *LC_CreateCache(unsigned long maxsize,int (*RestoreFunc)(struct lc_member *member,void *mem,unsigned long memberid))
{
	struct lc_cache *cache;

	if(!(cache=malloc(sizeof(struct lc_cache))))
	{
		fprintf(stderr,"PANIC: no memory for cache struct\n");
		exit(10L);
	}

	cache->first=0L;
	cache->currentsize=0L;
	cache->maxsize=maxsize;
	cache->accesscounter=0L;
	cache->RestoreFunc=RestoreFunc;
	return cache;
}

unsigned long LC_ChangeMaxSize(struct lc_cache *cache,unsigned long newsize)
{
	unsigned long oldsize=cache->maxsize;
	cache->maxsize=newsize;

	while(cache->currentsize>cache->maxsize)
		LC_FlushOldestMember(cache);

	return oldsize;
}

void LC_DeleteCache(struct lc_cache *cache)
{
	while(cache->first)
		LC_RemoveMember(cache,cache->first);

	if(cache->currentsize!=0)
	{
		fprintf(stderr,"PANIC: Cache sanity check failed\n");
		exit(10L);
	}

	free(cache);
}


struct lc_member *LC_AddMember(struct lc_cache *cache,unsigned long size,unsigned long memberid,unsigned long flags)
{
	struct lc_member *mem;

	if(!(mem=malloc(sizeof(struct lc_member))))
	{
		fprintf(stderr,"PANIC: no memory for cache member struct\n");
		exit(10L);
	}

	mem->size=size;
	mem->accesscounter=0;
	mem->memberid=memberid;
	mem->data=0L;
	mem->next=cache->first;
	if(mem->next!=0L)
		mem->next->prev=mem;
	mem->prev=0L;
	cache->first=mem;
	mem->flags=flags;
	if(flags&LCM_NOFLUSH)
	{
		mem->data=(void *)memberid; //HACK
	}
	else
		mem->flags|=LCM_FREE;
	
	return mem;
}

void LC_RemoveMember(struct lc_cache *cache,struct lc_member *member)
{
	if(!member)
		return;

	if(member->prev==0L)
		cache->first=member->next;
	else
		member->prev->next=member->next;

	if(member->next!=0L)
		member->next->prev=member->prev;

	if((member->data)&&(member->flags&LCM_FREE))
	{
		free(member->data);
		if(!(member->flags&LCM_NOFLUSH))
			cache->currentsize-=member->size;
		//fprintf(stderr,"Removed member cursize: %p %d\n",cache,cache->currentsize);
	}

	free(member);

}

void LC_FlushOldestMember(struct lc_cache *cache)
{
	struct lc_member *curmem,*oldmem;

	curmem=cache->first;
	oldmem=0L;

	while(curmem!=0L)
	{
		if((!(curmem->flags&LCM_NOFLUSH))&&curmem->data)
		{
			if(oldmem==0L)
				oldmem=curmem;
			else
				if(curmem->accesscounter<oldmem->accesscounter)
					oldmem=curmem;
		}
		curmem=curmem->next;
	}

	if(oldmem==0L)
	{
		fprintf(stderr,"PANIC: no cache member to flush\n");
		exit(10L);
	}

	free(oldmem->data);
	oldmem->data=0L;

	cache->currentsize-=oldmem->size;
	//fprintf(stderr,"Flushed member cursize: %p %d\n",cache,cache->currentsize);
}

void *LC_AccessMember(struct lc_cache *cache,struct lc_member *member,unsigned long flags)
{
	if(!member)
		return 0L;

	if(member->data)
	{
		//Data is in memory
		member->accesscounter=cache->accesscounter;
		cache->accesscounter++;
		return member->data;
	}

	while((cache->currentsize+member->size)>cache->maxsize)
	{
		//Cache full -> free some memory
		LC_FlushOldestMember(cache);		
	}

	member->data=malloc(member->size);

	if(member->data==0L)
	{
		fprintf(stderr,"PANIC: no memory for cache member data\n");
		exit(10L);
	}
	
	
	if(!(flags&LCA_NORESTORE))
		if(!(cache->RestoreFunc(member,member->data,member->memberid)))
			fprintf(stderr,"WARNING: could not restore cache data\n");

	member->accesscounter=cache->accesscounter;
	cache->accesscounter++;
	cache->currentsize+=member->size;
	//fprintf(stderr,"Restored member cursize: %p %d\n",cache,cache->currentsize);
	return member->data;
}
