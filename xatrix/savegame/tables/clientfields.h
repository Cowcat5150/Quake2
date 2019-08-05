/*
 * =======================================================================
 *
 * Fields of the client to be saved.
 *
 * =======================================================================
 */

#ifdef __VBCC__

{"pers.weapon", CLOFSP(pers,weapon), F_ITEM},  	  
{"pers.lastweapon", CLOFSP(pers,lastweapon), F_ITEM},

#else

{"pers.weapon", CLOFS(pers.weapon), F_ITEM},
{"pers.lastweapon", CLOFS(pers.lastweapon), F_ITEM},

#endif

{"newweapon", CLOFS(newweapon), F_ITEM},
{NULL, 0, F_INT}
