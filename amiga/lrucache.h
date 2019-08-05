/*****************************************************************
* 21-Dec-1999 PA -    Generic LRU-Caching                        *
*                     Initial version                            *
* (Hyperion Software)                                            *
*****************************************************************/

#ifndef LRUCACHE_H
#define LRUCACHE_H

struct lc_member
{
	unsigned long size;
	struct lc_member *next;
	struct lc_member *prev;
	void *data;
	unsigned long accesscounter;
	unsigned long memberid;
	unsigned long flags;
};

struct lc_cache
{
	struct lc_member *first;
	unsigned long currentsize;
	unsigned long maxsize;
	unsigned long accesscounter;
	int (*RestoreFunc)(struct lc_member *member,void *mem,unsigned long memberid);
};

struct lc_cache *LC_CreateCache(unsigned long maxsize,int (*RestoreFunc)(struct lc_member *member,void *mem,unsigned long memberid));
unsigned long LC_ChangeMaxSize(struct lc_cache *cache,unsigned long newsize);
void LC_DeleteCache(struct lc_cache *cache);

struct lc_member *LC_AddMember(struct lc_cache *cache,unsigned long size,unsigned long memberid,unsigned long flags);
void LC_RemoveMember(struct lc_cache *cache,struct lc_member *member);
void *LC_AccessMember(struct lc_cache *cache,struct lc_member *member,unsigned long flags);


// LC_AddMember flags
#define LCM_NOFLUSH (1L<<0)
#define LCM_FREE (1L<<1)

// LC_AccessMember flags
#define LCA_NORESTORE (1L<<0)

#endif
