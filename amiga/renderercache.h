#ifndef RENDERERCACHE_H
#define RENDERERCACHE_H

#ifdef __VBCC__
#pragma amiga-align
#elif defined(WARPUP)
#pragma pack(push,2)
#endif

#include <exec/exec.h>
#include <exec/lists.h>

#ifdef __VBCC__
#pragma default-align
#elif defined(WARPUP)
#pragma pack(pop)
#endif

struct soft_member
{
	struct 	soft_member *next;
	struct 	soft_member *prev;
	unsigned long 	size;
	void 		*data;
	unsigned long 	numaccesses;
	unsigned long 	id;
	unsigned long 	flags;
};

struct soft_cache
{
	struct 	soft_member *first;
	unsigned long 	currentsize;
	unsigned long 	maxsize;
	unsigned long 	numaccesses;
	int (*ReloadFunc)(struct soft_member *member,void* mem,unsigned long id);
};

typedef struct hard_node_s
{
    	struct Node 	CacheNode;
    	unsigned 	int size;
    	short 		is_swapped;
    	short 		in_list;
    	void 		*data;

} hard_node;

typedef void (*CacheFunc)(void *, int);

typedef struct
{
    	struct MinList 	CacheList;
    	hard_node 	*cache_entries;
    	CacheFunc 	Swap;
    	CacheFunc 	Reload;
    	unsigned 	int maxsize;
    	unsigned 	int currentsize;

} hard_cache;

void  HARD_AddMember(hard_cache *cache, int idnum, unsigned int size, void *data);
void  HARD_RemoveMember(hard_cache *cache, int idnum);
void  HARD_AccessMember(hard_cache *cache, int idnum);
hard_cache* HARD_CreateCache(unsigned int maxsize, unsigned int numentries, CacheFunc Swap, CacheFunc Reload);
void  HARD_DeleteCache(hard_cache *cache);
void  HARD_ResizeCache(hard_cache *cache, unsigned int newsize);

struct soft_member *SOFT_AddMember(struct soft_cache *cache,unsigned long size,unsigned long id,unsigned long flags);
void SOFT_RemoveMember(struct soft_cache *cache,struct soft_member *member);
void *SOFT_AccessMember(struct soft_cache *cache,struct soft_member *member,unsigned long flags);
struct soft_cache *SOFT_CreateCache(unsigned long maxsize,int (*RestoreFunc)(struct soft_member *member,void *mem,unsigned long id));
void SOFT_DeleteCache(struct soft_cache *cache);
unsigned long SOFT_ChangeMaxSize(struct soft_cache *cache,unsigned long newsize);
  
#define SOFT_FREE (1L<<0)
#define SOFT_NOFLUSH (1L<<1)
#define SOFT_NORESTORE (1L<<2)

#endif
