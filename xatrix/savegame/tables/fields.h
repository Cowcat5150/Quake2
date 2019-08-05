/*
 * =======================================================================
 *
 * Game fields to be saved.
 *
 * =======================================================================
 */ 

{"classname", FOFS(classname), F_LSTRING},
{"model", FOFS(model), F_LSTRING},
{"spawnflags", FOFS(spawnflags), F_INT},
{"speed", FOFS(speed), F_FLOAT},
{"accel", FOFS(accel), F_FLOAT},
{"decel", FOFS(decel), F_FLOAT},
{"target", FOFS(target), F_LSTRING},
{"targetname", FOFS(targetname), F_LSTRING},
{"pathtarget", FOFS(pathtarget), F_LSTRING},
{"deathtarget", FOFS(deathtarget), F_LSTRING},
{"killtarget", FOFS(killtarget), F_LSTRING},
{"combattarget", FOFS(combattarget), F_LSTRING},
{"message", FOFS(message), F_LSTRING},
{"team", FOFS(team), F_LSTRING},
{"wait", FOFS(wait), F_FLOAT},
{"delay", FOFS(delay), F_FLOAT},
{"random", FOFS(random), F_FLOAT},
{"move_origin", FOFS(move_origin), F_VECTOR},
{"move_angles", FOFS(move_angles), F_VECTOR},
{"style", FOFS(style), F_INT},
{"count", FOFS(count), F_INT},
{"health", FOFS(health), F_INT},
{"sounds", FOFS(sounds), F_INT},
{"light", 0, F_IGNORE},
{"dmg", FOFS(dmg), F_INT},
{"mass", FOFS(mass), F_INT},
{"volume", FOFS(volume), F_FLOAT},
{"attenuation", FOFS(attenuation), F_FLOAT},
{"map", FOFS(map), F_LSTRING},

#ifdef __VBCC__

{"origin", FOFSES(s,origin), F_VECTOR},
{"angles", FOFSES(s,angles), F_VECTOR},
{"angle", FOFSES(s,angles), F_ANGLEHACK},

#else

{"origin", FOFS(s.origin), F_VECTOR},
{"angles", FOFS(s.angles), F_VECTOR},
{"angle", FOFS(s.angles), F_ANGLEHACK},

#endif

{"goalentity", FOFS(goalentity), F_EDICT, FFL_NOSPAWN},
{"movetarget", FOFS(movetarget), F_EDICT, FFL_NOSPAWN},
{"enemy", FOFS(enemy), F_EDICT, FFL_NOSPAWN},
{"oldenemy", FOFS(oldenemy), F_EDICT, FFL_NOSPAWN},
{"activator", FOFS(activator), F_EDICT, FFL_NOSPAWN},
{"groundentity", FOFS(groundentity), F_EDICT, FFL_NOSPAWN},
{"teamchain", FOFS(teamchain), F_EDICT, FFL_NOSPAWN},
{"teammaster", FOFS(teammaster), F_EDICT, FFL_NOSPAWN},
{"owner", FOFS(owner), F_EDICT, FFL_NOSPAWN},
{"mynoise", FOFS(mynoise), F_EDICT, FFL_NOSPAWN},
{"mynoise2", FOFS(mynoise2), F_EDICT, FFL_NOSPAWN},
{"target_ent", FOFS(target_ent), F_EDICT, FFL_NOSPAWN},
{"chain", FOFS(chain), F_EDICT, FFL_NOSPAWN},
{"prethink", FOFS(prethink), F_FUNCTION, FFL_NOSPAWN},
{"think", FOFS(think), F_FUNCTION, FFL_NOSPAWN},
{"blocked", FOFS(blocked), F_FUNCTION, FFL_NOSPAWN},
{"touch", FOFS(touch), F_FUNCTION, FFL_NOSPAWN},
{"use", FOFS(use), F_FUNCTION, FFL_NOSPAWN},
{"pain", FOFS(pain), F_FUNCTION, FFL_NOSPAWN},
{"die", FOFS(die), F_FUNCTION, FFL_NOSPAWN},

#ifdef __VBCC__

{"stand", FOFSMO(monsterinfo,stand), F_FUNCTION, FFL_NOSPAWN},
{"idle", FOFSMO(monsterinfo,idle), F_FUNCTION, FFL_NOSPAWN},
{"search", FOFSMO(monsterinfo,search), F_FUNCTION, FFL_NOSPAWN},
{"walk", FOFSMO(monsterinfo,walk), F_FUNCTION, FFL_NOSPAWN},
{"run", FOFSMO(monsterinfo,run), F_FUNCTION, FFL_NOSPAWN},
{"dodge", FOFSMO(monsterinfo,dodge), F_FUNCTION, FFL_NOSPAWN},
{"attack", FOFSMO(monsterinfo,attack), F_FUNCTION, FFL_NOSPAWN},
{"melee", FOFSMO(monsterinfo,melee), F_FUNCTION, FFL_NOSPAWN},
{"sight", FOFSMO(monsterinfo,sight), F_FUNCTION, FFL_NOSPAWN},
{"checkattack", FOFSMO(monsterinfo,checkattack), F_FUNCTION, FFL_NOSPAWN},
{"currentmove", FOFSMO(monsterinfo,currentmove), F_MMOVE, FFL_NOSPAWN},
{"endfunc", FOFSMV(moveinfo,endfunc), F_FUNCTION, FFL_NOSPAWN},  

#else

{"stand", FOFS(monsterinfo.stand), F_FUNCTION, FFL_NOSPAWN},
{"idle", FOFS(monsterinfo.idle), F_FUNCTION, FFL_NOSPAWN},
{"search", FOFS(monsterinfo.search), F_FUNCTION, FFL_NOSPAWN},
{"walk", FOFS(monsterinfo.walk), F_FUNCTION, FFL_NOSPAWN},
{"run", FOFS(monsterinfo.run), F_FUNCTION, FFL_NOSPAWN},
{"dodge", FOFS(monsterinfo.dodge), F_FUNCTION, FFL_NOSPAWN},
{"attack", FOFS(monsterinfo.attack), F_FUNCTION, FFL_NOSPAWN},
{"melee", FOFS(monsterinfo.melee), F_FUNCTION, FFL_NOSPAWN},
{"sight", FOFS(monsterinfo.sight), F_FUNCTION, FFL_NOSPAWN},
{"checkattack", FOFS(monsterinfo.checkattack), F_FUNCTION, FFL_NOSPAWN},
{"currentmove", FOFS(monsterinfo.currentmove), F_MMOVE, FFL_NOSPAWN},
{"endfunc", FOFS(moveinfo.endfunc), F_FUNCTION, FFL_NOSPAWN},

#endif

{"lip", STOFS(lip), F_INT, FFL_SPAWNTEMP},
{"distance", STOFS(distance), F_INT, FFL_SPAWNTEMP},
{"height", STOFS(height), F_INT, FFL_SPAWNTEMP},
{"noise", STOFS(noise), F_LSTRING, FFL_SPAWNTEMP},
{"pausetime", STOFS(pausetime), F_FLOAT, FFL_SPAWNTEMP},
{"item", STOFS(item), F_LSTRING, FFL_SPAWNTEMP},
{"item", FOFS(item), F_ITEM},
{"gravity", STOFS(gravity), F_LSTRING, FFL_SPAWNTEMP},
{"sky", STOFS(sky), F_LSTRING, FFL_SPAWNTEMP},
{"skyrotate", STOFS(skyrotate), F_FLOAT, FFL_SPAWNTEMP},
{"skyaxis", STOFS(skyaxis), F_VECTOR, FFL_SPAWNTEMP},
{"minyaw", STOFS(minyaw), F_FLOAT, FFL_SPAWNTEMP},
{"maxyaw", STOFS(maxyaw), F_FLOAT, FFL_SPAWNTEMP},
{"minpitch", STOFS(minpitch), F_FLOAT, FFL_SPAWNTEMP},
{"maxpitch", STOFS(maxpitch), F_FLOAT, FFL_SPAWNTEMP},
{"nextmap", STOFS(nextmap), F_LSTRING, FFL_SPAWNTEMP},
{0, 0, 0, 0}
