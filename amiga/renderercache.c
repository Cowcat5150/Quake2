
#include <stdlib.h>
#include <stdio.h>

#ifdef __VBCC__
#pragma amiga-align
#elif defined(WARPUP)
#pragma pack(push,2)
#endif

#include <clib/alib_protos.h> // Cowcat
#include <proto/exec.h>	      //
#include <proto/dos.h>

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

#include "renderercache.h"
#include "../ref_soft/r_local.h"

#ifdef __VBCC__

#ifdef __PPC__
#define ADDHEAD AddHeadPPC
#define REMOVE  RemovePPC
#define NEWLIST NewListPPC
#define REMTAIL RemTailPPC
#else
#define ADDHEAD AddHead
#define REMOVE	Remove
#define NEWLIST NewList
#define REMTAIL RemTail
#endif

#else // gnu

//struct Node *nodebase;

//extern struct Node *RmTailPPC( void *, struct Node *, struct List * );
//#define _RmTailPPC(node, list) RmTailPPC(PowerPCBase, node, list)

#undef ADDHEAD
#undef REMOVE
#undef NEWLIST
#undef REMTAIL
#define ADDHEAD AddHeadPPC
#define REMOVE  RemovePPC
#define NEWLIST NewListPPC
//#define REMTAIL _RmTailPPC
#define REMTAIL RemTailPPC

#endif

// Software Renderer accesses cache through a memory handle,
// while Hardware Renderer accesses cache through a ID Number.
// So there are two LRU Cache implementations, to make integration
// of the Caching into each renderer easier to do.

void SOFT_FlushOldestMember(struct soft_cache *cache);
char filename[32];

#ifdef __VBCC__
#define __inline inline
#endif

static __inline void HARD_MaybeFreeSomeMembers(hard_cache *cache, int size)
{    
	hard_node *member;
	hard_node *node;

	while (cache->currentsize + size > cache->maxsize)
	{
		//node = (hard_node *)REMTAIL((struct Node *)nodebase, (struct List *)&(cache->CacheList));
		node = (hard_node *)REMTAIL((struct List *)&(cache->CacheList));

		if (node)
			cache->currentsize -= node->size;

		node->CacheNode.ln_Succ = NULL;
		node->CacheNode.ln_Pred = NULL;
		node->in_list = 0;
		member = node;

		if (member == NULL)
		{
			fprintf(stderr, "ERROR: Out of memory\n");
			exit(10L);
		}

		cache->Swap(member->data, (int)(member-cache->cache_entries));
		member->is_swapped = 1;
	}
}

void HARD_AddMember(hard_cache *cache, int idnum, unsigned int size, void *data)
{
	hard_node *node;

	if (!cache)
		return;

	node = &cache->cache_entries[idnum];

	if ((node->size == size) && (node->data == data))
		return;

	node->is_swapped = 0;
	node->size = size;
	node->data = data;

	HARD_MaybeFreeSomeMembers(cache, size);

	if (node->in_list==0)
	{
		ADDHEAD((struct List *)&(cache->CacheList),&(node->CacheNode));
		cache->currentsize += node->size;
		node->in_list = 1;
	}
}

void HARD_RemoveMember(hard_cache *cache, int idnum)
{
	hard_node *node;

	if (!cache)
		return;

	node  = &cache->cache_entries[idnum];

	if ((node->CacheNode.ln_Succ != NULL) && (node->CacheNode.ln_Pred != NULL))
	{
		if (node)
			cache->currentsize -= node->size;

		REMOVE(&(node->CacheNode));
		node->CacheNode.ln_Succ = NULL;
		node->CacheNode.ln_Pred = NULL;
		node->in_list = 0;
	}

	node->data = NULL;
	node->size = 0;
}

void HARD_AccessMember(hard_cache *cache, int id)
{
	hard_node * node;

	if (!cache)
		return;

	node = &cache->cache_entries[id];

	if (node->is_swapped)
	{
		HARD_MaybeFreeSomeMembers(cache, node->size);
		cache->Reload(node->data, id);
		node->is_swapped = 0;

		if (node->in_list==0)
		{
			ADDHEAD((struct List *)&(cache->CacheList),&(node->CacheNode));
			cache->currentsize += node->size;
			node->in_list = 1;
		}
	}

	else
	{
		if ((hard_node *)cache->CacheList.mlh_Head != node)
		{
			if (node->CacheNode.ln_Succ != NULL)
			{
				REMOVE(&(node->CacheNode));
				ADDHEAD((struct List *)&(cache->CacheList),&(node->CacheNode));
			}
		}
	}
}

hard_cache* HARD_CreateCache(unsigned int maxsize, unsigned int numentries, CacheFunc Swap, CacheFunc Reload)
{
	int size,i;
	hard_cache *cache;
	size = sizeof(hard_node)*numentries + sizeof(hard_cache);
	cache = malloc(size);

	if (cache)
	{
		NEWLIST((struct List *)&(cache->CacheList));
		cache->cache_entries = (hard_node *)(cache+1);
		cache->maxsize = maxsize;
		cache->currentsize = 0;
		cache->Swap = Swap;
		cache->Reload = Reload;

		for (i=0; i<numentries; i++)
		{
			memset(&cache->cache_entries[i], 0, sizeof(hard_node));
		}
	}

	return cache;
}

void HARD_DeleteCache(hard_cache *cache)
{
	if (!cache)
		return;

	free(cache);
	cache=0;	
}

void HARD_ResizeCache(hard_cache *cache, unsigned int newsize)
{
	if (!cache)
		return;

	if (newsize < 1000000)
		return;

	cache->maxsize = newsize;
	HARD_MaybeFreeSomeMembers(cache, 0);
}

struct soft_member *SOFT_AddMember(struct soft_cache *cache,unsigned long size,unsigned long id,unsigned long flags)
{
	struct soft_member *mem;

	if(!(mem=malloc(sizeof(struct soft_member))))
	{
		fprintf(stderr,"ERROR: Out of Memory\n");
		exit(10L);
	}

	mem->next=cache->first;

	if(mem->next!=0L)
		mem->next->prev=mem;

	mem->prev=0L;
	cache->first=mem;
	mem->flags=flags;
	mem->data=0L;
  
	if(flags&SOFT_NOFLUSH)
		mem->data=(void *)id;

	else
		mem->flags|=SOFT_FREE;

	mem->size=size;
	mem->numaccesses=0;
	mem->id=id;

	return mem;
}

void SOFT_RemoveMember(struct soft_cache *cache,struct soft_member *member)
{
	if(!member)
		return;

	if(!cache)
		return;

	if(member->prev==0L)
		cache->first=member->next;

	else
		member->prev->next=member->next;

	if(member->next!=0L)
		member->next->prev=member->prev;

	if((member->data)&&(member->flags&SOFT_FREE))
	{
		free(member->data);
			member->data=0;

		if(!(member->flags&SOFT_NOFLUSH))
			cache->currentsize-=member->size;
	}

	free(member);
	member=0;
}

void *SOFT_AccessMember(struct soft_cache *cache,struct soft_member *member,unsigned long flags)
{
	if(!member)
		return 0L;

	if(member->data)
	{
		member->numaccesses=cache->numaccesses;
		cache->numaccesses++;
		return member->data;
	}

	while((cache->currentsize+member->size)>cache->maxsize)
	{
		SOFT_FlushOldestMember(cache);
	}

	member->data=malloc(member->size);

	if(member->data==0L)
	{
		fprintf(stderr,"ERROR: Out of memory\n");
		exit(10L);
	}


	if(!(flags&SOFT_NORESTORE))
	{
		if(!(cache->ReloadFunc(member,member->data,member->id)))
			fprintf(stderr,"WARNING: could not restore data\n");
	}

	member->numaccesses=cache->numaccesses;
	cache->numaccesses++;
	cache->currentsize+=member->size;
	return member->data;
}


struct soft_cache *SOFT_CreateCache(unsigned long maxsize,int (*ReloadFunc)(struct soft_member *member,void *mem,unsigned long id))
{
	struct soft_cache *cache;

	if(!(cache=malloc(sizeof(struct soft_cache))))
	{
		fprintf(stderr,"ERROR: no memory for cache\n");
		exit(10L);
	}

	cache->numaccesses=0L;
	cache->ReloadFunc=ReloadFunc;
	cache->first=0L;
	cache->currentsize=0L;
	cache->maxsize=maxsize;	 
	return cache;
}

void SOFT_DeleteCache(struct soft_cache *cache)
{
	while(cache->first)
		SOFT_RemoveMember(cache,cache->first);

	if(cache->currentsize != 0)
	{
		fprintf(stderr,"ERROR: Cache corrupt\n");
		exit(10L);
	}

	free(cache);
	cache = 0;
}

unsigned long SOFT_ChangeMaxSize(struct soft_cache *cache,unsigned long newsize)
{
	unsigned long oldsize=cache->maxsize;
	cache->maxsize = newsize;

	while(cache->currentsize>cache->maxsize)
		SOFT_FlushOldestMember(cache);

	return oldsize;
}
       
void SOFT_FlushOldestMember(struct soft_cache *cache)
{
	struct soft_member *curmem,*oldmem;

	curmem = cache->first;
	oldmem = 0L;

	while(curmem != 0L)
	{
		if((!(curmem->flags&SOFT_NOFLUSH))&&curmem->data)
		{
			if(oldmem == 0L)
				oldmem=curmem;

			else if(curmem->numaccesses<oldmem->numaccesses)
				oldmem=curmem;
		}

		curmem=curmem->next;
	}

	if(oldmem == 0L)
	{
		fprintf(stderr,"ERROR: no cache member exists\n");
		exit(10L);
	}

	free(oldmem->data);
	oldmem->data = 0L;

	cache->currentsize-=oldmem->size;
}
  
