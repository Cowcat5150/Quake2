/*
 * Copyright (C) 1997-2001 Id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * =======================================================================
 *
 * Jump in into the game.so and support functions.
 *
 * =======================================================================
 */

#include "header/local.h"

#ifdef AMIGAOS

#include "../amiga/dll.h"

#ifdef __VBCC__
#pragma amiga-align
#elif defined(WARPUP)
#pragma pack(push,2)
#endif

#include <proto/exec.h>
#include <proto/dos.h>
#include <dos/dosextens.h>

#ifdef __VBCC__
#pragma default-align
#elif defined(WARPUP)
#pragma pack(pop)
#endif

#endif

game_locals_t game;
level_locals_t level;
game_import_t gi;
game_export_t globals;
spawn_temp_t st;

int sm_meat_index;
int snd_fry;
int meansOfDeath;

edict_t *g_edicts;

cvar_t *deathmatch;
cvar_t *coop;
cvar_t *dmflags;
cvar_t *skill;
cvar_t *fraglimit;
cvar_t *timelimit;
cvar_t *capturelimit;
cvar_t *instantweap;
cvar_t *password;
cvar_t *maxclients;
cvar_t *maxentities;
cvar_t *g_select_empty;
cvar_t *dedicated;

cvar_t *filterban;

cvar_t *sv_maxvelocity;
cvar_t *sv_gravity;

cvar_t *sv_rollspeed;
cvar_t *sv_rollangle;
cvar_t *gun_x;
cvar_t *gun_y;
cvar_t *gun_z;

cvar_t *run_pitch;
cvar_t *run_roll;
cvar_t *bob_up;
cvar_t *bob_pitch;
cvar_t *bob_roll;

cvar_t *sv_cheats;

cvar_t *flood_msgs;
cvar_t *flood_persecond;
cvar_t *flood_waitdelay;

cvar_t *sv_maplist;

DLLFUNC void SpawnEntities(char *mapname, char *entities, char *spawnpoint);
DLLFUNC void ClientThink(edict_t *ent, usercmd_t *cmd);
DLLFUNC qboolean ClientConnect(edict_t *ent, char *userinfo);
DLLFUNC void ClientUserinfoChanged(edict_t *ent, char *userinfo);
DLLFUNC void ClientDisconnect(edict_t *ent);
DLLFUNC void ClientBegin(edict_t *ent);
DLLFUNC void ClientCommand(edict_t *ent);
//void RunEntity(edict_t *ent);
DLLFUNC void WriteGame(char *filename, qboolean autosave);
DLLFUNC void ReadGame(char *filename);
DLLFUNC void WriteLevel(char *filename);
DLLFUNC void ReadLevel(char *filename);
DLLFUNC void InitGame(void);
DLLFUNC void G_RunFrame(void);

/* =================================================================== */

DLLFUNC void ShutdownGame(void)
{
	gi.dprintf("==== ShutdownGame ====\n");

	gi.FreeTags(TAG_LEVEL);
	gi.FreeTags(TAG_GAME);
}

/*
 * Returns a pointer to the structure with
 * all entry points and global variables
 */

DLLFUNC game_export_t *GetGameAPI(game_import_t *import)
{
	gi = *import;

	globals.apiversion = GAME_API_VERSION;
	globals.Init = InitGame;
	globals.Shutdown = ShutdownGame;
	globals.SpawnEntities = SpawnEntities;

	globals.WriteGame = WriteGame;
	globals.ReadGame = ReadGame;
	globals.WriteLevel = WriteLevel;
	globals.ReadLevel = ReadLevel;

	globals.ClientThink = ClientThink;
	globals.ClientConnect = ClientConnect;
	globals.ClientUserinfoChanged = ClientUserinfoChanged;
	globals.ClientDisconnect = ClientDisconnect;
	globals.ClientBegin = ClientBegin;
	globals.ClientCommand = ClientCommand;

	globals.RunFrame = G_RunFrame;

	globals.ServerCommand = ServerCommand;

	globals.edict_size = sizeof(edict_t);

	return &globals;
}

#ifndef GAME_HARD_LINKED
void Sys_Error(char *error, ...)
{
	va_list argptr;
	char text[1024];

	va_start(argptr, error);
	vsprintf(text, error, argptr);
	va_end(argptr);

	gi.error(ERR_FATAL, "%s", text);
}

void Com_Printf(char *msg, ...)
{
	va_list argptr;
	char text[1024];

	va_start(argptr, msg);
	vsprintf(text, msg, argptr);
	va_end(argptr);

	gi.dprintf("%s", text);
}
#endif

/* ====================================================================== */

void ClientEndServerFrames(void)
{
	int i;
	edict_t *ent;

	/* calc the player views now that all
	   pushing  and damage has been added */
	for (i = 0; i < maxclients->value; i++)
	{
		ent = g_edicts + 1 + i;

		if (!ent->inuse || !ent->client)
		{
			continue;
		}

		ClientEndServerFrame(ent);
	}
}

/*
 * Returns the created target changelevel
 */
edict_t *CreateTargetChangeLevel(char *map)
{
	edict_t *ent;

	ent = G_Spawn();
	ent->classname = "target_changelevel";
	Com_sprintf(level.nextmap, sizeof(level.nextmap), "%s", map);
	ent->map = level.nextmap;
	return ent;
}

/*
 * The timelimit or fraglimit has been exceeded
 */
void EndDMLevel(void)
{
	edict_t *ent;
	char *s, *t, *f;
	static const char *seps = " ,\n\r";

	/* stay on same level flag */
	if ((int)dmflags->value & DF_SAME_LEVEL)
	{
		BeginIntermission(CreateTargetChangeLevel(level.mapname));
		return;
	}

	if (*level.forcemap)
	{
		BeginIntermission(CreateTargetChangeLevel(level.forcemap));
		return;
	}

	/* see if it's in the map list */
	if (*sv_maplist->string)
	{
		s = strdup(sv_maplist->string);
		f = NULL;
		t = strtok(s, seps);

		while (t != NULL)
		{
			if (Q_strcasecmp(t, level.mapname) == 0)
			{
				/* it's in the list, go to the next one */
				t = strtok(NULL, seps);

				if (t == NULL) /* end of list, go to first one */
				{
					if (f == NULL) /* there isn't a first one, same level */
					{
						BeginIntermission(CreateTargetChangeLevel(level.mapname));
					}
					else
					{
						BeginIntermission(CreateTargetChangeLevel(f));
					}
				}
				else
				{
					BeginIntermission(CreateTargetChangeLevel(t));
				}

				free(s);
				return;
			}

			if (!f)
			{
				f = t;
			}

			t = strtok(NULL, seps);
		}

		free(s);
	}

	if (level.nextmap[0]) /* go to a specific map */
	{
		BeginIntermission(CreateTargetChangeLevel(level.nextmap));
	}
	else /* search for a changelevel */
	{
		ent = G_Find(NULL, FOFS(classname), "target_changelevel");

		if (!ent)
		{
			/* the map designer didn't include a changelevel,
			   so create a fake ent that goes back to the same level */
			BeginIntermission(CreateTargetChangeLevel(level.mapname));
			return;
		}

		BeginIntermission(ent);
	}
}

void CheckDMRules(void)
{
	int 		i;
	gclient_t 	*cl;

	if (level.intermissiontime)
	{
		return;
	}

	if (!deathmatch->value)
	{
		return;
	}

	if (ctf->value && CTFCheckRules())
	{
		EndDMLevel();
		return;
	}

	if (CTFInMatch())
	{
		return; /* no checking in match mode */
	}

	if (timelimit->value)
	{
		if (level.time >= timelimit->value * 60)
		{
			gi.bprintf(PRINT_HIGH, "Timelimit hit.\n");
			EndDMLevel();
			return;
		}
	}

	if (fraglimit->value)
	{
		for (i = 0; i < maxclients->value; i++)
		{
			cl = game.clients + i;

			if (!g_edicts[i + 1].inuse)
			{
				continue;
			}

			if (cl->resp.score >= fraglimit->value)
			{
				gi.bprintf(PRINT_HIGH, "Fraglimit hit.\n");
				EndDMLevel();
				return;
			}
		}
	}
}

void ExitLevel(void)
{
	int 		i;
	edict_t 	*ent;
	char 		command[256];

	level.exitintermission = 0;
	level.intermissiontime = 0;

	if (CTFNextMap())
	{
		return;
	}

	Com_sprintf(command, sizeof(command), "gamemap \"%s\"\n", level.changemap);
	gi.AddCommandString(command);
	ClientEndServerFrames();

	level.changemap = NULL;

	/* clear some things before going to next level */
	for (i = 0; i < maxclients->value; i++)
	{
		ent = g_edicts + 1 + i;

		if (!ent->inuse)
		{
			continue;
		}

		if (ent->health > ent->client->pers.max_health)
		{
			ent->health = ent->client->pers.max_health;
		}
	}

	gibsthisframe = 0;
	lastgibframe = 0;
}

/*
 * Advances the world by 0.1 seconds
 */

DLLFUNC void G_RunFrame(void)
{
	int i;
	edict_t *ent;

	level.framenum++;
	level.time = level.framenum * FRAMETIME;

	/* choose a client for monsters to target this frame */
	AI_SetSightClient();

	/* exit intermissions */

	if (level.exitintermission)
	{
		ExitLevel();
		return;
	}

	/* treat each object in turn even
	   the world gets a chance to think */
	ent = &g_edicts[0];

	for (i = 0; i < globals.num_edicts; i++, ent++)
	{
		if (!ent->inuse)
		{
			continue;
		}

		level.current_entity = ent;

		VectorCopy(ent->s.origin, ent->s.old_origin);

		/* if the ground entity moved, make sure we are still on it */
		if ((ent->groundentity) && (ent->groundentity->linkcount != ent->groundentity_linkcount))
		{
			ent->groundentity = NULL;

			if (!(ent->flags & (FL_SWIM | FL_FLY)) && (ent->svflags & SVF_MONSTER))
			{
				M_CheckGround(ent);
			}
		}

		if ((i > 0) && (i <= maxclients->value))
		{
			ClientBeginServerFrame(ent);
			continue;
		}

		G_RunEntity(ent);
	}

	/* see if it is time to end a deathmatch */
	CheckDMRules();

	/* build the playerstate_t structures for all players */
	ClientEndServerFrames();
}

#ifdef AMIGAOS

#if 0
DLLFUNC void* dllFindResource(int id, char *pType)
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

ULONG SegList;

dll_tExportSymbol DLL_ExportSymbols[] =
{
    	//{(void *)dllFindResource, "dllFindResource"},
	//{(void *)dllLoadResource, "dllLoadResource"},
    	//{(void *)dllFreeResource, "dllFreeResource"},
    	{(void *)GetGameAPI, "GetGameAPI"},
    	{0,0}
};

dll_tImportSymbol DLL_ImportSymbols[] =
{
    	{0,0,0,0}
};

int DLL_Init(void)
{
    	struct CommandLineInterface *pCLI = Cli();

    	if (!pCLI)      // Who the f*ck started us ?
		return 0;

    	SegList = (ULONG)(pCLI->cli_Module);

    	return 1;
}

void DLL_DeInit(void)
{
}

#ifdef __GNUC__
extern int main(int, char **); // new Cowcat
#endif

#endif /* AMIGA */
