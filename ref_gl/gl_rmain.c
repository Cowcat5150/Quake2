//surgeon: added fast procedure for R_Polyblend
//added shademodel optimization for worldmodel
//all entities are state-sorted for best performance

/* 
Copyright (C) 1997-2001 Id Software, Inc. 
 
This program is free software; you can redistribute it and/or 
modify it under the terms of the GNU General Public License 
as published by the Free Software Foundation; either version 2 
of the License, or (at your option) any later version. 
 
This program is distributed in the hope that it will be useful, 
but WITHOUT ANY WARRANTY; without even the implied warranty of 
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.   
 
See the GNU General Public License for more details. 
 
You should have received a copy of the GNU General Public License 
along with this program; if not, write to the Free Software 
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA. 
 
*/ 
// r_main.c

#include "gl_local.h" 
#include <ctype.h> // Cowcat
 
void R_Clear (void); 
 
viddef_t        vid; 
 
refimport_t     ri; 
 
int GL_TEXTURE0, GL_TEXTURE1; 

extern qboolean arb_multitexture; //surgeon
#ifdef AMIGAOS
extern qboolean mgl_arb_multitexture; //surgeon
#endif
 
model_t         *r_worldmodel; 
 
float           gldepthmin, gldepthmax; 
 
glconfig_t 	gl_config; 
glstate_t  	gl_state; 
 
image_t         *r_notexture;           // use for bad textures 
image_t         *r_particletexture;     // little dot for particles 
 
entity_t        *currententity; 
model_t         *currentmodel; 
 
cplane_t        frustum[4]; 
 
int             r_visframecount;        // bumped when going to a new PVS 
int             r_framecount;           // used for dlight push checking 
 
int             c_brush_polys, c_alias_polys; 
 
float           v_blend[4];             // final blending color 
 
void GL_Strings_f( void ); 
 
// 
// view origin 
// 
vec3_t  vup; 
vec3_t  vpn; 
vec3_t  vright; 
vec3_t  r_origin; 
 
float   r_world_matrix[16]; 
float   r_base_world_matrix[16]; 
 
// 
// screen size info 
// 
refdef_t r_newrefdef; 
 
int	r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2; 
 
cvar_t  *r_norefresh; 
cvar_t  *r_drawentities; 
cvar_t  *r_drawworld; 
cvar_t  *r_speeds; 
cvar_t  *r_fullbright; 
cvar_t  *r_novis; 
cvar_t  *r_nocull; 
cvar_t  *r_lerpmodels; 
cvar_t  *r_lefthand; 
cvar_t  *r_lightlevel;  // FIXME: This is a HACK to get the client's light level 

#ifdef AMIGAOS //surgeon

extern cvar_t *gl_affinemodels;
extern cvar_t *gl_skipparticles;
extern cvar_t *gl_drawparticles; 	//moved
extern cvar_t *gl_point_size; 		//moved
cvar_t  *r_cullworld;
//cvar_t  *gl_ext_clip_volume_hint; 	//aliasmodel bounds // Disabled Cowcat
cvar_t  *r_vertexlighting;
cvar_t  *gl_polycull;
cvar_t  *gl_smoothmodels;
cvar_t  *gl_keeptjunctions;
//cvar_t  *gl_multitexturesort;

#endif
 
cvar_t  *gl_nosubimage; 
cvar_t  *gl_allow_software; 
 
cvar_t  *gl_vertex_arrays; 
 
cvar_t  *gl_particle_min_size; 
cvar_t  *gl_particle_max_size; 
cvar_t  *gl_particle_size; 
cvar_t  *gl_particle_att_a; 
cvar_t  *gl_particle_att_b; 
cvar_t  *gl_particle_att_c; 
 
cvar_t  *gl_ext_swapinterval; 
cvar_t  *gl_ext_palettedtexture; 
cvar_t  *gl_ext_multitexture; 
cvar_t  *gl_ext_pointparameters; 
cvar_t  *gl_ext_compiled_vertex_array; 
 
cvar_t  *gl_log; 
cvar_t  *gl_bitdepth; 
cvar_t  *gl_drawbuffer; 
cvar_t  *gl_driver; 
cvar_t  *gl_lightmap; 
cvar_t  *gl_shadows; 
cvar_t  *gl_mode; 
cvar_t  *gl_dynamic; 
cvar_t  *gl_monolightmap; 
cvar_t  *gl_modulate; 
cvar_t  *gl_nobind; 
cvar_t  *gl_round_down; 
cvar_t  *gl_picmip; 
cvar_t  *gl_skymip; 
cvar_t  *gl_showtris; 
cvar_t  *gl_ztrick; 
cvar_t  *gl_finish; 
cvar_t  *gl_clear; 
cvar_t  *gl_cull; 
cvar_t  *gl_polyblend; 
cvar_t  *gl_flashblend; 
cvar_t  *gl_playermip; 
cvar_t  *gl_saturatelighting; 
cvar_t  *gl_swapinterval; 
cvar_t  *gl_texturemode; 
cvar_t  *gl_texturealphamode; 
cvar_t  *gl_texturesolidmode; 
cvar_t  *gl_lockpvs; 
 
cvar_t  *gl_3dlabs_broken; 
 
cvar_t  *vid_fullscreen; 
cvar_t  *vid_gamma; 
cvar_t  *vid_ref; 

// fog Cowcat
cvar_t	*gl_fog;
cvar_t	*gl_fogred;
cvar_t	*gl_foggreen;
cvar_t	*gl_fogblue;
cvar_t	*gl_fogstart;
cvar_t	*gl_fogend;
//cvar_t *gl_fogdensity;
qboolean fog = false; 

cvar_t	*gl_showbbox;
cvar_t	*gl_nolerp_list;

extern void *qwglGetProcAddress(char *x); // Cowcat

 /* 
================= 
R_CullBox 
 
Returns true if the box is completely outside the frustom 
================= 
*/ 

#ifndef ALT_BOPS
//surgeon: made a macro in q_shared.h

qboolean R_CullBox (vec3_t mins, vec3_t maxs) 
{ 
	int             i; 

	if (r_nocull->value) 
		return false; 

	for (i=0 ; i<4 ; i++)
		if ( BoxOnPlaneSide(mins, maxs, &frustum[i]) == 2) // BOX_ON_PLANE_SIDE macro never met ? - Cowcat
			return true;

	return false;
 
} 
#endif 
 
void R_RotateForEntity (entity_t *e) 
{ 
    	qglTranslatef (e->origin[0],  e->origin[1],  e->origin[2]); 

	#ifndef AMIGAOS

    	qglRotatef (e->angles[1],  0, 0, 1); 
    	qglRotatef (-e->angles[0],  0, 1, 0); 
    	qglRotatef (-e->angles[2],  1, 0, 0); 

	#else //use specialized common case MiniGL rotation-routines

    	glRotatefEXT (e->angles[1],  GLROT_001); 
    	glRotatefEXT (-e->angles[0], GLROT_010); 
    	glRotatefEXT (-e->angles[2], GLROT_100); 

	#endif
} 
 
/* 
============================================================= 
 
  SPRITE MODELS 
 
============================================================= 
*/ 
 
 
/* 
================= 
R_DrawSpriteModel 
 
================= 
*/

void R_DrawSpriteModel (entity_t *e) 
{ 
	float 		alpha = 1.0F; 
	vec3_t  	point; 
	dsprframe_t     *frame; 
	float           *up, *right; 
	dsprite_t       *psprite; 
 
	// don't even bother culling, because it's just a single 
	// polygon without a surface cache
 
	#ifdef AMIGAOS

	psprite = *(dsprite_t **)currentmodel->extradata; 

	#else 

	psprite = (dsprite_t *)currentmodel->extradata; 

	#endif 
 
#if 0 
	if (e->frame < 0 || e->frame >= psprite->numframes) 
	{ 
		ri.Con_Printf (PRINT_ALL, "no such sprite frame %i\n", e->frame); 
		e->frame = 0; 
	} 
#endif
 
	e->frame %= psprite->numframes; 
 
	frame = &psprite->frames[e->frame]; 
 
#if 0 
	if (psprite->type == SPR_ORIENTED) 
	{       // bullet marks on walls 
		vec3_t	v_forward, v_right, v_up; 
 
		AngleVectors (currententity->angles, v_forward, v_right, v_up); 
		up = v_up; 
		right = v_right; 
	} 

	else 
#endif 
	{       // normal sprite 
		up = vup; 
		right = vright; 
	} 
 
	if ( e->flags & RF_TRANSLUCENT ) 
		alpha = e->alpha; 
 
	qglColor4f( 1, 1, 1, alpha ); 
 
	GL_Bind(currentmodel->skins[e->frame]->texnum); 
 
	#ifndef AMIGAOS //done globally for all translucent ents

	GL_TexEnv( GL_MODULATE ); 

	if ( alpha != 1.0F ) 
		qglEnable( GL_BLEND ); 

	#endif 
 
	if ( alpha == 1.0 ) 
		qglEnable (GL_ALPHA_TEST); 

	else 
		qglDisable( GL_ALPHA_TEST );
 
 
	qglBegin (GL_QUADS); 
 
	qglTexCoord2f (0, 1); 
	VectorMA (e->origin, -frame->origin_y, up, point); 
	VectorMA (point, -frame->origin_x, right, point); 
	qglVertex3fv (point); 
 
	qglTexCoord2f (0, 0); 
	VectorMA (e->origin, frame->height - frame->origin_y, up, point); 
	VectorMA (point, -frame->origin_x, right, point); 
	qglVertex3fv (point); 
 
	qglTexCoord2f (1, 0); 
	VectorMA (e->origin, frame->height - frame->origin_y, up, point); 
	VectorMA (point, frame->width - frame->origin_x, right, point); 
	qglVertex3fv (point); 
 
	qglTexCoord2f (1, 1); 
	VectorMA (e->origin, -frame->origin_y, up, point); 
	VectorMA (point, frame->width - frame->origin_x, right, point); 
	qglVertex3fv (point); 
	 
	qglEnd (); 
 
	qglDisable (GL_ALPHA_TEST); 

	#ifndef AMIGAOS //done globally for all translucent ents

	GL_TexEnv( GL_REPLACE ); 
 
	if ( alpha != 1.0F ) 
		qglDisable( GL_BLEND ); 

	#endif 

	qglColor4f( 1, 1, 1, 1 ); 
} 
 
//================================================================================== 
 
/* 
============= 
R_DrawNullModel 
============= 
*/ 

void R_DrawNullModel (void) 
{ 
	vec3_t  shadelight; 
	int     i; 
 
	if ( currententity->flags & RF_FULLBRIGHT ) 
		shadelight[0] = shadelight[1] = shadelight[2] = 1.0F; 

	else 
		R_LightPoint (currententity->origin, shadelight); 
 
    	qglPushMatrix (); 
	R_RotateForEntity (currententity); 
 
	qglDisable (GL_TEXTURE_2D);
 
	qglColor3fv (shadelight); 
 
	qglBegin (GL_TRIANGLE_FAN); 

	qglVertex3f (0, 0, -16);
 
	for (i=0 ; i<=4 ; i++) 
		qglVertex3f (16*cos(i*M_PI/2), 16*sin(i*M_PI/2), 0); 

	qglEnd (); 
 
	qglBegin (GL_TRIANGLE_FAN); 

	qglVertex3f (0, 0, 16); 

	for (i=4 ; i>=0 ; i--) 
		qglVertex3f (16*cos(i*M_PI/2), 16*sin(i*M_PI/2), 0); 

	qglEnd (); 
 
	qglColor3f (1,1,1); 
	qglPopMatrix (); 
	qglEnable (GL_TEXTURE_2D); 
} 

#ifdef AMIGAOS //surgeon: sorting for entities + other stuff

/* 
============= 
R_DrawEntitiesOnList 
============= 
*/ 

void R_DrawEntitiesOnList (void) 
{ 
	int             i; 
	qboolean	translucent;
	//qboolean	maybe_mtex;

	if (!r_drawentities->value) 
		return; 

	translucent = false;
	//maybe_mtex = false;
 
	// draw non-transparent first 

	if(!r_vertexlighting->value)
		qglShadeModel(GL_FLAT);

	else
		qglShadeModel(GL_SMOOTH);

	for (i=0 ; i<r_newrefdef.num_entities ; i++) 
	{ 
		currententity = &r_newrefdef.entities[i];
 
		if (currententity->flags & (RF_TRANSLUCENT | RF_BEAM))
		{ 
			translucent = true;
			continue;       // solid 
 		}

		else 
		{ 
			currentmodel = currententity->model;
 
			if (!currentmodel) 
			{ 
				R_DrawNullModel (); 
				continue; 
			}
 
			switch (currentmodel->type) 
			{ 
				case mod_alias:  
					break; 

				case mod_brush: 
					//maybe_mtex = true;
					R_DrawBrushModel (currententity);
					break;
 
				case mod_sprite: 
					R_DrawSpriteModel (currententity); 
					break;
 
				default: 
					ri.Sys_Error (ERR_DROP, "Bad modeltype"); 
					break; 
			} 
		} 
	} 

#if 0 //obsolete
	if(mgl_arb_multitexture && maybe_mtex) //added 09_AUG_02
	{
		//draw multitextured rushmodels
		extern void R_DrawMTexBuffer(void);
		maybe_mtex = false; //clear
		R_DrawMTexBuffer(); //brushmodels
	}
#endif

	GL_TexEnv( GL_MODULATE ); 

	if(gl_smoothmodels->value)
		qglShadeModel(GL_SMOOTH);

	else
		qglShadeModel(GL_FLAT);

	if(gl_affinemodels->value)
		glDisable(MGL_PERSPECTIVE_MAPPING);

	for (i=0 ; i<r_newrefdef.num_entities ; i++) 
	{ 
		currententity = &r_newrefdef.entities[i];
 
		if (currententity->flags & (RF_TRANSLUCENT | RF_BEAM)) 
			continue; 
  
		currentmodel = currententity->model; 

		if (!currentmodel) 
			continue;
 
		if(currentmodel->type != mod_alias)
			continue;

		R_DrawAliasModel (currententity); 
	} 

	if(gl_affinemodels->value)
		glEnable(MGL_PERSPECTIVE_MAPPING);


	if( !translucent )
	{
		GL_TexEnv( GL_REPLACE ); 
		if(!gl_smoothmodels->value)
		qglShadeModel(GL_SMOOTH);

		return;
	}

	//surgeon: wee keep modulate because most translucent entities needs it
	//surgeon: blending is switched on globally for all translucent ents:

	qglEnable(GL_BLEND);

	// draw transparent entities 
	// we could sort these if it ever becomes a problem... 

	qglShadeModel(GL_FLAT);
	qglDepthMask (0);	// no z writes
 
	for (i=0 ; i<r_newrefdef.num_entities ; i++) 
	{ 
		currententity = &r_newrefdef.entities[i];
 
		if (!(currententity->flags & (RF_TRANSLUCENT | RF_BEAM))) 
			continue;       // solid 
 
		if ( currententity->flags & RF_BEAM ) 
		{ 
			R_DrawBeam( currententity ); 
		} 

		else 
		{ 
			currentmodel = currententity->model; 
 
			if (!currentmodel) 
			{ 
				R_DrawNullModel (); 
				continue; 
			} 

			switch (currentmodel->type) 
			{ 
				case mod_alias: 
					break;
 
				case mod_brush: 
					R_DrawBrushModel (currententity); 
					break; 

				case mod_sprite: 
					R_DrawSpriteModel (currententity); 
					break; 

				default: 
					ri.Sys_Error (ERR_DROP, "Bad modeltype"); 
					break; 
			} 
		} 
	}

	// GL_TexEnv( GL_MODULATE ); 

	if(gl_smoothmodels->value)
		qglShadeModel(GL_SMOOTH);

	else
		qglShadeModel(GL_FLAT);

	if(gl_affinemodels->value)
		glDisable(MGL_PERSPECTIVE_MAPPING);

	for (i=0 ; i<r_newrefdef.num_entities ; i++) 
	{ 
		currententity = &r_newrefdef.entities[i];
 
		if (!(currententity->flags & RF_TRANSLUCENT))
			continue;       // solid 

		if(currententity->flags & RF_BEAM)
			continue; 
 
 		currentmodel = currententity->model; 
 
		if (!currentmodel) 
			continue;

		if(currentmodel->type != mod_alias)
			continue;

		R_DrawAliasModel (currententity); 
	}

	//surgeon: blending is switched off globally for all translucent ents:
	qglDisable(GL_BLEND);

	if(gl_affinemodels->value)
		glEnable(MGL_PERSPECTIVE_MAPPING);

	qglDepthMask (1);	// back to writing 

	qglShadeModel(GL_SMOOTH);

	GL_TexEnv( GL_REPLACE ); 
} 

#else

/* 
============= 
R_DrawEntitiesOnList 
============= 
*/
 
void R_DrawEntitiesOnList (void) 
{ 
	int	i; 
 
	if (!r_drawentities->value) 
		return; 
 
	// draw non-transparent first 
	for (i=0 ; i<r_newrefdef.num_entities ; i++) 
	{ 
		currententity = &r_newrefdef.entities[i];

		if (currententity->flags & RF_TRANSLUCENT) 
			continue;       // solid 
 
		if ( currententity->flags & RF_BEAM ) 
		{ 
			R_DrawBeam( currententity ); 
		}
 
		else 
		{ 
			currentmodel = currententity->model;

			if (!currentmodel) 
			{ 
				R_DrawNullModel (); 
				continue; 
			}

			switch (currentmodel->type) 
			{ 
				case mod_alias: 
					R_DrawAliasModel (currententity); 
					break;

				case mod_brush:
					R_DrawBrushModel (currententity); 
					break;
 
				case mod_sprite:
					R_DrawSpriteModel (currententity); 
					break;

				default: 
					ri.Sys_Error (ERR_DROP, "Bad modeltype"); 
					break; 
			} 
		} 
	} 
 
	// draw transparent entities 
	// we could sort these if it ever becomes a problem... 

	qglDepthMask (0);	// no z writes 

	for (i=0 ; i<r_newrefdef.num_entities ; i++) 
	{ 
		currententity = &r_newrefdef.entities[i];
 
		if (!(currententity->flags & RF_TRANSLUCENT)) 
			continue;       // solid 
 
		if ( currententity->flags & RF_BEAM ) 
		{ 
			R_DrawBeam( currententity ); 
		}
 
		else 
		{ 
			currentmodel = currententity->model; 
 
			if (!currentmodel) 
			{ 
				R_DrawNullModel (); 
				continue; 
			}

			switch (currentmodel->type) 
			{ 
				case mod_alias: 
					R_DrawAliasModel (currententity); 
					break;

				case mod_brush: 
					R_DrawBrushModel (currententity); 
					break;

				case mod_sprite: 
					R_DrawSpriteModel (currententity); 
					break;
 
				default: 
					ri.Sys_Error (ERR_DROP, "Bad modeltype"); 
					break; 
			} 
		} 
	}

	qglDepthMask (1);               // back to writing 
 
} 
#endif 

/* 
** GL_DrawParticles 
** 
*/

#define PARTICLE_BATCH_SIZE 400
 
void GL_DrawParticles( int num_particles, const particle_t particles[], const unsigned colortable[768] ) 
{ 
	const particle_t	*p; 
	int			i; 
	vec3_t			up, right; 
	float			scale; 
	byte			color[4]; 
 	int			num_batches, remainder;

	#ifdef AMIGAOS

	int p_interval;
	p_interval = 1 + (int)gl_skipparticles->value;

	#endif

	GL_Bind(r_particletexture->texnum); 
	qglDepthMask( GL_FALSE );  // no z buffering 
	qglEnable( GL_BLEND ); 
	GL_TexEnv( GL_MODULATE );
 
	VectorScale (vup, 1.5, up); 
	VectorScale (vright, 1.5, right); 

	num_batches = num_particles / PARTICLE_BATCH_SIZE;
	remainder = num_particles % PARTICLE_BATCH_SIZE; 

	p = particles;

	while ( num_batches )
	{
		qglBegin( GL_TRIANGLES ); 

	#ifndef AMIGAOS
		for (i=0 ; i < PARTICLE_BATCH_SIZE ; i++, p++) 
	#else
		for (i=0 ; i <= PARTICLE_BATCH_SIZE-p_interval; i+=p_interval, p+=p_interval) 
	#endif
		{ 
			// hack a scale up to keep particles from disapearing 
			scale = ( p->origin[0] - r_origin[0] ) * vpn[0] +  
			    	( p->origin[1] - r_origin[1] ) * vpn[1] + 
			    	( p->origin[2] - r_origin[2] ) * vpn[2]; 
 
			if (scale < 20) 
				scale = 1; 

			else 
				scale = 1 + scale * 0.004; 
 
			*(int *)color = colortable[p->color]; 
			color[3] = p->alpha*255; 
 
			qglColor4ubv( color ); 
 
			qglTexCoord2f( 0.0625, 0.0625 ); 
			qglVertex3fv( p->origin ); 
 	
			qglTexCoord2f( 1.0625, 0.0625 ); 
			qglVertex3f( p->origin[0] + up[0]*scale,  
				p->origin[1] + up[1]*scale,  
					 p->origin[2] + up[2]*scale); 
 
			qglTexCoord2f( 0.0625, 1.0625 ); 
			qglVertex3f( p->origin[0] + right[0]*scale,  
				 p->origin[1] + right[1]*scale,  
					 p->origin[2] + right[2]*scale); 
		} 
 
		qglEnd ();

		num_batches--;
	}

	if(remainder)
	{
		qglBegin( GL_TRIANGLES ); 

	#ifndef AMIGAOS
		for (i=0 ; i < remainder ; i++, p++) 
	#else
		for (i=0 ; i <= remainder-p_interval ; i+=p_interval, p+=p_interval) 
	#endif
		{ 
			// hack a scale up to keep particles from disapearing 
			scale = ( p->origin[0] - r_origin[0] ) * vpn[0] +  
			    	( p->origin[1] - r_origin[1] ) * vpn[1] + 
			    	( p->origin[2] - r_origin[2] ) * vpn[2]; 
 
			if (scale < 20) 
				scale = 1;
 
			else 
				scale = 1 + scale * 0.004; 
 
			*(int *)color = colortable[p->color]; 
			color[3] = p->alpha*255; 
 
			qglColor4ubv( color ); 
 
			qglTexCoord2f( 0.0625, 0.0625 ); 
			qglVertex3fv( p->origin ); 
 
			qglTexCoord2f( 1.0625, 0.0625 ); 
			qglVertex3f( p->origin[0] + up[0]*scale,  
				 p->origin[1] + up[1]*scale,  
					 p->origin[2] + up[2]*scale); 
 
			qglTexCoord2f( 0.0625, 1.0625 ); 
			qglVertex3f( p->origin[0] + right[0]*scale,  
				 p->origin[1] + right[1]*scale,  
					 p->origin[2] + right[2]*scale); 
		}

		qglEnd ();
	} 
 
	qglDisable( GL_BLEND ); 
	qglColor4f( 1,1,1,1 ); 
	qglDepthMask( 1 );              // back to normal Z buffering 
	GL_TexEnv( GL_REPLACE ); 
}
 
/* 
=============== 
R_DrawParticles 
=============== 
*/

void R_DrawParticles (void) 
{ 
	#ifdef AMIGAOS

	// extern cvar_t *gl_drawparticles;
 
	if (gl_drawparticles->value == 0) 
	    return; 

	if(r_newrefdef.num_particles == 0) 
		return; //surgeon

	qglDisable(GL_CULL_FACE); //surgeon

	if (gl_drawparticles->value == 2) 
	{ 

	#else

	if ( gl_ext_pointparameters->value && qglPointParameterfEXT ) 
	{ 

	#endif
		int i; 
		unsigned char color[4]; 
		const particle_t *p; 

		#ifdef AMIGAOS

		int p_interval;
		p_interval = 1 + (int)gl_skipparticles->value;

		#endif
 
		qglDepthMask( GL_FALSE ); 
		qglEnable( GL_BLEND ); 
		qglDisable( GL_TEXTURE_2D ); 
 
		qglPointSize( gl_point_size->value ); 
 
		qglBegin( GL_POINTS ); 

		#ifndef AMIGAOS

		for ( i = 0, p = r_newrefdef.particles; i < r_newrefdef.num_particles; i++, p++ ) 

		#else

		for ( i = 0, p = r_newrefdef.particles; i <= r_newrefdef.num_particles-p_interval; i+=p_interval, p+=p_interval ) 

		#endif
		{ 
			*(int *)color = d_8to24table[p->color]; 
			color[3] = p->alpha*255; 
 
			qglColor4ubv( color ); 
 
			qglVertex3fv( p->origin ); 
		}
 
		qglEnd(); 
 
		qglDisable( GL_BLEND ); 
		qglColor4f( 1.0F, 1.0F, 1.0F, 1.0F ); 
		qglDepthMask( GL_TRUE ); 
		qglEnable( GL_TEXTURE_2D );
	} 

	else 
	{ 
		GL_DrawParticles( r_newrefdef.num_particles, r_newrefdef.particles, d_8to24table ); 
	} 

	#ifdef AMIGAOS

	if (gl_cull->value)
		qglEnable(GL_CULL_FACE); //Surgeon

	#endif
} 
 
/* 
============ 
R_PolyBlend 
============ 
*/ 

void R_PolyBlend (void) 
{ 
	if (!gl_polyblend->value) 
		return;
 
	if (!v_blend[3]) 
		return; 
 
	qglDisable (GL_ALPHA_TEST); 
	qglEnable (GL_BLEND); 
	qglDisable (GL_DEPTH_TEST); 
	qglDisable (GL_TEXTURE_2D); 
 
	#ifndef AMIGAOS //surgeon

    	qglLoadIdentity (); 
 
	// FIXME: get rid of these 
    	qglRotatef (-90,  1, 0, 0);     // put Z going up 
    	qglRotatef (90,  0, 0, 1);      // put Z going up 

	#endif
 
	qglColor4fv (v_blend); 

	#ifndef AMIGAOS //surgeon 

	qglBegin (GL_QUADS); 
 
	qglVertex3f (10, 100, 100); 
	qglVertex3f (10, -100, 100); 
	qglVertex3f (10, -100, -100); 
	qglVertex3f (10, 100, -100); 
	qglEnd (); 

	#else

	qglBegin (MGL_FLATFAN);
	qglVertex2f (0, 0);
	qglVertex2f (vid.width, 0);
	qglVertex2f (vid.width, vid.height);
	qglVertex2f (0, vid.height);
	qglEnd ();

	#endif 

	qglDisable (GL_BLEND); 
	qglEnable (GL_TEXTURE_2D); 
	qglEnable (GL_ALPHA_TEST); 
 
	qglColor4f(1,1,1,1); 
} 
 
//======================================================================= 
 
int SignbitsForPlane (cplane_t *out) 
{ 
	int     bits, j; 
 
	// for fast box on planeside test 
 
	bits = 0; 

	for (j=0 ; j<3 ; j++) 
	{ 
		if (out->normal[j] < 0) 
			bits |= 1<<j; 
	} 
	return bits; 
} 

static void TurnVector (vec3_t out, const vec3_t forward, const vec3_t side, float angle)
{
	float	scale_forward, scale_side;

	#if 1

	scale_forward = cos(angle * M_PI / 180.0);
	scale_side = sin(angle * M_PI / 180.0);

	#else

	SinCos((angle * M_PI / 180.0), &scale_side, &scale_forward);

	#endif

	out[0] = scale_forward*forward[0] + scale_side*side[0];
	out[1] = scale_forward*forward[1] + scale_side*side[1];
	out[2] = scale_forward*forward[2] + scale_side*side[2];
}

void R_SetFrustum (void) 
{ 
	int	i; 

	#if 1  // Cowcat optimization

	/*
	float vpnrx, vpnry;

	vpnrx = (90-r_newrefdef.fov_x / 2);
	vpnry = (90-r_newrefdef.fov_y / 2);
	
	// rotate VPN right by FOV_X/2 degrees 
	RotatePointAroundVector( frustum[0].normal, vup, vpn, -vpnrx ); 
	// rotate VPN left by FOV_X/2 degrees 
	RotatePointAroundVector( frustum[1].normal, vup, vpn, vpnrx ); 
	// rotate VPN up by FOV_X/2 degrees 
	RotatePointAroundVector( frustum[2].normal, vright, vpn, vpnry ); 
	// rotate VPN down by FOV_X/2 degrees 
	RotatePointAroundVector( frustum[3].normal, vright, vpn, -vpnry ); 
	*/

	float vpnrx = r_newrefdef.fov_x * 0.5;
	float vpnry = r_newrefdef.fov_y * 0.5;
		
	TurnVector(frustum[0].normal, vpn, vright, vpnrx - 90); // left plane
	TurnVector(frustum[1].normal, vpn, vright, 90 - vpnrx); // right plane
	TurnVector(frustum[2].normal, vpn, vup,    90 - vpnry); // bottom plane
	TurnVector(frustum[3].normal, vpn, vup,    vpnry - 90); // top plane
	//ri.Con_Printf (PRINT_ALL, "frust.normal %f, %f, %f,%f\n", frustum[0].normal,frustum[1].normal,frustum[2].normal,frustum[3].normal);

	#else 

	// rotate VPN right by FOV_X/2 degrees 
	RotatePointAroundVector( frustum[0].normal, vup, vpn, -(90-r_newrefdef.fov_x / 2 ) ); 
	// rotate VPN left by FOV_X/2 degrees 
	RotatePointAroundVector( frustum[1].normal, vup, vpn, 90-r_newrefdef.fov_x / 2 ); 
	// rotate VPN up by FOV_X/2 degrees 
	RotatePointAroundVector( frustum[2].normal, vright, vpn, 90-r_newrefdef.fov_y / 2); 
	// rotate VPN down by FOV_X/2 degrees 
	RotatePointAroundVector( frustum[3].normal, vright, vpn, -( 90 - r_newrefdef.fov_y / 2) ); 

	#endif 
 

	#ifdef ALT_BOPS
	//surgeon: fast culling nicked from darkplaces

	for (i=0 ; i<4 ; i++) 
	{ 
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal);
		BoxOnPlaneSideClassify(&frustum[i]);
	}

	#else

	for (i=0 ; i<4 ; i++) 
	{ 
		frustum[i].type = PLANE_ANYZ; 
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal); 
		frustum[i].signbits = SignbitsForPlane (&frustum[i]); 
	}

	#endif 
} 
 
//======================================================================= 
 
/* 
=============== 
R_SetupFrame 
=============== 
*/ 

void R_SetupFrame (void) 
{ 
	int 	i; 
	mleaf_t *leaf; 
 
	r_framecount++; 
 
	// build the transformation matrix for the given view angles 
	VectorCopy (r_newrefdef.vieworg, r_origin); 
 
	AngleVectors (r_newrefdef.viewangles, vpn, vright, vup); 
 
	// current viewcluster 
	if ( !( r_newrefdef.rdflags & RDF_NOWORLDMODEL ) ) 
	{ 
		r_oldviewcluster = r_viewcluster; 
		r_oldviewcluster2 = r_viewcluster2; 
		leaf = Mod_PointInLeaf (r_origin, r_worldmodel); 
		r_viewcluster = r_viewcluster2 = leaf->cluster; 
 
		// check above and below so crossing solid water doesn't draw wrong 
		if (!leaf->contents) 
		{ 
			// look down a bit 
			vec3_t  temp; 
 
			VectorCopy (r_origin, temp); 
			temp[2] -= 16; 
			leaf = Mod_PointInLeaf (temp, r_worldmodel);
 
			if ( !(leaf->contents & CONTENTS_SOLID) && (leaf->cluster != r_viewcluster2) ) 
				r_viewcluster2 = leaf->cluster; 
		} 

		else 
		{
			// look up a bit 
			vec3_t  temp; 
 
			VectorCopy (r_origin, temp); 
			temp[2] += 16; 
			leaf = Mod_PointInLeaf (temp, r_worldmodel);
 
			if ( !(leaf->contents & CONTENTS_SOLID) && (leaf->cluster != r_viewcluster2) ) 
				r_viewcluster2 = leaf->cluster; 
		} 
	} 
 
	for (i=0 ; i<4 ; i++) 
		v_blend[i] = r_newrefdef.blend[i]; 
 
	c_brush_polys = 0; 
	c_alias_polys = 0; 

	#if 0 //workaround for MiniGL bug

	if ( r_newrefdef.rdflags & RDF_NOWORLDMODEL ) 
	{
		GLfloat x[4];
		GLfloat y[4];

		qglDisable(GL_TEXTURE_2D);
		qglColor4f( 0.3f, 0.3f, 0.3f, 1.0f);

		x[0] = x[1] = (float)r_newrefdef.x;
		x[2] = x[3] = (float)r_newrefdef.width + x[0];

		y[0] = y[3] = (float)r_newrefdef.y;
		y[1] = y[2] = (float)r_newrefdef.height + y[0];
 
		qglBegin(MGL_FLATFAN);
		qglVertex3f(x[0], y[0], 1.0f); 
		qglVertex3f(x[1], y[1], 1.0f); 
		qglVertex3f(x[2], y[2], 1.0f); 
		qglVertex3f(x[3], y[3], 1.0f);
		qglEnd(); 

		qglColor4f(1.f, 1.f, 1.f, 1.f); 
		qglEnable(GL_TEXTURE_2D);

	}

	#else 

	// clear out the portion of the screen that the NOWORLDMODEL defines 
	if ( r_newrefdef.rdflags & RDF_NOWORLDMODEL ) 
	{ 
		qglEnable( GL_SCISSOR_TEST ); 
		qglClearColor( 0.3, 0.3, 0.3, 1 ); 
		qglScissor( r_newrefdef.x, vid.height - r_newrefdef.height - r_newrefdef.y, r_newrefdef.width, r_newrefdef.height ); 

		#ifdef AMIGAOS

		if (gl_config.renderer == GL_RENDERER_PERMEDIA2) 
		{		
		    	//surgeon: texturing must be disabled !

		    	qglDisable(GL_TEXTURE_2D);
 
		    	qglColor4f( 0.3f, 0.3f, 0.3f, 1.0f); 

		    	qglBegin(MGL_FLATFAN); // Work around a bug in the permedia2 driver 
			qglVertex3f(0.0f, 0.0f, 1.0f); 
			qglVertex3f(0.0f, (GLfloat)vid.height, 1.0f); 
			qglVertex3f((GLfloat)vid.width, (GLfloat)vid.height, 1.0f); 
			qglVertex3f((GLfloat)vid.width, 0.0f, 1.0f);
		    	qglEnd(); 

		    	qglColor4f(1.f, 1.f, 1.f, 1.f); 

		    	//surgeon:
		    	qglEnable(GL_TEXTURE_2D);
		} 

		else 
		{ 
		    	qglClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ); 
		}
 
		#else 

		qglClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ); 

		#endif
 
		//surgeon: was (1.0 0.0, 0.5, 0.5) yuck!
		qglClearColor( 0.0, 0.0, 0.0, 1.0 );
		qglDisable( GL_SCISSOR_TEST );
 	} 
#endif

} 
 
void MYgluPerspective( GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar ) 
{ 
   	GLdouble xmin, xmax, ymin, ymax; 
 
   	ymax = zNear * tan( fovy * M_PI / 360.0 ); // (M_PI / 360) );  // Cowcat
   	ymin = -ymax; 
 
   	xmin = ymin * aspect; 
   	xmax = ymax * aspect; 

#ifdef STEREO 
   	xmin += -( 2 * gl_state.camera_separation ) / zNear; 
   	xmax += -( 2 * gl_state.camera_separation ) / zNear; 
#endif
 
   	qglFrustum( xmin, xmax, ymin, ymax, zNear, zFar ); 
} 
 
 
/* 
============= 
R_SetupGL 
============= 
*/ 

void R_SetupGL (void) 
{ 
	float   screenaspect; 
//	float   yfov; 
	int     x, x2, y2, y, w, h; 

	#ifdef AMIGAOS //surgeon

	extern cvar_t *gl_mintriarea;

	#endif

	// 
	// set up viewport 
	// 

#if 0   // Cowcat (borrowed from BlitzQuake - slow here? )

	int dwidth,dheight;

	dwidth = vid.width / vid.width;
	dheight = vid.height / vid.height;

	x = floor(r_newrefdef.x * dwidth); 
	x2 = ceil((r_newrefdef.x + r_newrefdef.width) * dwidth); 
	y = floor(vid.height - r_newrefdef.y * dheight); 
	y2 = ceil(vid.height - (r_newrefdef.y + r_newrefdef.height) * dheight);
	
#else
	x = floor(r_newrefdef.x * vid.width / vid.width); 
	x2 = ceil((r_newrefdef.x + r_newrefdef.width) * vid.width / vid.width); 
	y = floor(vid.height - r_newrefdef.y * vid.height / vid.height); 
	y2 = ceil(vid.height - (r_newrefdef.y + r_newrefdef.height) * vid.height / vid.height);
 
#endif
 
	w = x2 - x; 
	h = y - y2; 
 
	qglViewport (x, y2, w, h); 
 
	// 
	// set up projection matrix 
	//
 
    	screenaspect = (float)r_newrefdef.width/r_newrefdef.height; 

	// yfov = 2*atan((float)r_newrefdef.height/r_newrefdef.width)*180/M_PI; 

	qglMatrixMode(GL_PROJECTION); 
    	qglLoadIdentity (); 
    	MYgluPerspective (r_newrefdef.fov_y,  screenaspect,  4,  4096); 
 
	qglCullFace(GL_FRONT); 
 
	qglMatrixMode(GL_MODELVIEW); 
    	qglLoadIdentity (); 

	#ifndef AMIGAOS

    	qglRotatef (-90,  1, 0, 0);     // put Z going up 
    	qglRotatef (90,  0, 0, 1);      // put Z going up 
    	qglRotatef (-r_newrefdef.viewangles[2],  1, 0, 0); 
    	qglRotatef (-r_newrefdef.viewangles[0],  0, 1, 0); 
    	qglRotatef (-r_newrefdef.viewangles[1],  0, 0, 1);

	#else //use specialized common case MiniGL rotation-routines

    	glRotatefEXTs (-1, 0,  GLROT_100);  // put Z going up 
    	glRotatefEXTs ( 1, 0,  GLROT_001);  // put Z going up 
    	glRotatefEXT (-r_newrefdef.viewangles[2],  GLROT_100); 
    	glRotatefEXT (-r_newrefdef.viewangles[0],  GLROT_010); 
    	glRotatefEXT (-r_newrefdef.viewangles[1],  GLROT_001);

	#endif

    	qglTranslatef (-r_newrefdef.vieworg[0],  -r_newrefdef.vieworg[1],  -r_newrefdef.vieworg[2]); 
 
#ifdef STEREO
	if ( gl_state.camera_separation != 0 && gl_state.stereo_enabled ) 
             qglTranslatef ( gl_state.camera_separation, 0, 0 ); 
#endif
 
	qglGetFloatv (GL_MODELVIEW_MATRIX, r_world_matrix); 
 
	// 
	// set drawing parms 
	//

	if (gl_cull->value) 
		qglEnable(GL_CULL_FACE);

	else 
		qglDisable(GL_CULL_FACE); 
 
	qglDisable(GL_BLEND); 
	qglDisable(GL_ALPHA_TEST); 
	qglEnable(GL_DEPTH_TEST); 

	#ifdef AMIGAOS //surgeon

	//clamp particle draw to valid range
	if(gl_drawparticles->value < 0)
		ri.Cvar_SetValue( "gl_drawparticles", 0 ); 

	if(gl_drawparticles->value > 2)
		ri.Cvar_SetValue( "gl_drawparticles", 2 );

	//clamp particle-skipper to valid range
	if(gl_skipparticles->value < 0)
		ri.Cvar_SetValue( "gl_skipparticles", 0 ); 

	if(gl_skipparticles->value > 4)
		ri.Cvar_SetValue( "gl_skipparticles", 4 );

	//clamp the triangle elimination area
	if(gl_mintriarea->value < 0.5)
		ri.Cvar_SetValue( "gl_mintriarea", 0.5 ); 

	if(gl_mintriarea->value > 10.0)
		ri.Cvar_SetValue( "gl_mintriarea", 10.0 ); 

	mglMinTriArea( gl_mintriarea->value );

	#endif
} 
 
/* 
============= 
R_Clear 
============= 
*/

void R_Clear (void) 
{
	GLbitfield clearBits = 0;

	if (gl_clear->value) 
		clearBits |= GL_COLOR_BUFFER_BIT; 
 
	if (gl_ztrick->value) 
	{ 
		static int trickframe; 
 
		//if (gl_clear->value) 
			//qglClear (GL_COLOR_BUFFER_BIT); 
 
		trickframe++;

		if (trickframe & 1) 
		{ 
			gldepthmin = 0; 
			gldepthmax = 0.49999; 
			qglDepthFunc (GL_LEQUAL); 
		}

		else 
		{ 
			gldepthmin = 1; 
			gldepthmax = 0.5; 
			qglDepthFunc (GL_GEQUAL); 
		} 
	}

	else 
	{ 
		//if (gl_clear->value) 
			//qglClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
 
		//else 
			//qglClear (GL_DEPTH_BUFFER_BIT);

		clearBits |= GL_DEPTH_BUFFER_BIT;

		gldepthmin = 0; 
		gldepthmax = 1; 
		qglDepthFunc (GL_LEQUAL); 
	} 
 
	qglDepthRange (gldepthmin, gldepthmax);

	if (clearBits)
		qglClear(clearBits);
 
} 

/*
void R_Flash( void ) 
{ 
	R_PolyBlend (); 
} 
*/
 
/* 
================ 
R_RenderView 
 
r_newrefdef must be set before the first call 
================ 
*/

void R_RenderView (refdef_t *fd) 
{
	vec3_t	fogcolors;	// Cowcat Fog

	if (r_norefresh->value) 
		return; 
 
	r_newrefdef = *fd; 
 
	if (!r_worldmodel && !( r_newrefdef.rdflags & RDF_NOWORLDMODEL ) ) 
		ri.Sys_Error (ERR_DROP, "R_RenderView: NULL worldmodel"); 
 
	if (r_speeds->value) 
	{ 
		c_brush_polys = 0; 
		c_alias_polys = 0; 
	} 
 
	R_PushDlights (); 
 
	if (gl_finish->value) 
		qglFinish (); 
 
	R_SetupFrame (); 
 
	R_SetFrustum (); 
 
	R_SetupGL (); 
 
	R_MarkLeaves ();        // done here so we know if we're in water 
 
	#ifdef AMIGAOS //surgeon

	if(!r_vertexlighting->value)
		qglShadeModel(GL_FLAT);

	if(!gl_polycull->value)
		qglDisable(GL_CULL_FACE);

	#if 0

	if(gl_multitexturesort->value)
		mglMtexSort(GL_TRUE);

	else
		mglMtexSort(GL_FALSE);

	#endif

	#endif

	#if 1 // Cowcat - see DrawBeam, gl_warp.c/R_DrawSkyBox, gl_light.c/R_RenderDLights

	if (gl_fog->value)
	{			
		glFogi(GL_FOG_MODE, GL_LINEAR);

		fogcolors[0] = gl_fogred->value;
		fogcolors[1] = gl_foggreen->value;
		fogcolors[2] = gl_fogblue->value;

		glFogfv (GL_FOG_COLOR, fogcolors);		
		glFogf (GL_FOG_START, gl_fogstart->value);
		glFogf (GL_FOG_END, gl_fogend->value);

		qglEnable(GL_FOG);

		fog = true;	
		
	}

	else if (fog) // was fog
	{	
		qglDisable(GL_FOG);
		fog = false;
	}

	#endif
	
	R_DrawWorld (); 

	#ifdef AMIGAOS //surgeon
	if(!gl_polycull->value && gl_cull->value)
		qglEnable(GL_CULL_FACE);

	if(!r_vertexlighting->value)
		qglShadeModel(GL_SMOOTH);
	#endif
 
	R_DrawEntitiesOnList (); 

	R_RenderDlights (); 
 
	R_DrawParticles (); 
 
	R_DrawAlphaSurfaces (); 

	//R_Flash(); 
 	R_PolyBlend (); //Cowcat

	if (r_speeds->value) 
	{ 
		ri.Con_Printf (PRINT_ALL, "%4i wpoly %4i epoly %i tex %i lmaps\n", 
			c_brush_polys,  
			c_alias_polys,  
			c_visible_textures,  
			c_visible_lightmaps);  
	} 
} 
 
void R_SetGL2D (void) 
{ 
	// set 2D virtual screen size 
	qglViewport (0,0, vid.width, vid.height);

	#ifndef AMIGAOS // bypass transformation pipeline

	qglMatrixMode(GL_PROJECTION); 
    	qglLoadIdentity (); 
	qglOrtho  (0, vid.width, vid.height, 0, -99999, 99999); 
	qglMatrixMode(GL_MODELVIEW); 
    	qglLoadIdentity ();

	#endif

	qglDisable (GL_DEPTH_TEST); 
	qglDisable (GL_CULL_FACE); 
	qglDisable (GL_BLEND); 
	qglEnable (GL_ALPHA_TEST); 
	qglColor4f (1,1,1,1); 
} 
 

#ifdef STEREO

static void GL_DrawColoredStereoLinePair( float r, float g, float b, float y ) 
{ 
	qglColor3f( r, g, b ); 
	qglVertex2f( 0, y ); 
	qglVertex2f( vid.width, y ); 
	qglColor3f( 0, 0, 0 ); 
	qglVertex2f( 0, y + 1 ); 
	qglVertex2f( vid.width, y + 1 ); 
} 
 
static void GL_DrawStereoPattern( void ) 
{ 
	int i; 
 
	if ( !( gl_config.renderer & GL_RENDERER_INTERGRAPH ) ) 
		return; 
 
	if ( !gl_state.stereo_enabled ) 
		return; 
 
	R_SetGL2D(); 
 
	qglDrawBuffer( GL_BACK_LEFT ); 
 
	for ( i = 0; i < 20; i++ ) 
	{ 
		qglBegin( GL_LINES ); 
			GL_DrawColoredStereoLinePair( 1, 0, 0, 0 ); 
			GL_DrawColoredStereoLinePair( 1, 0, 0, 2 ); 
			GL_DrawColoredStereoLinePair( 1, 0, 0, 4 ); 
			GL_DrawColoredStereoLinePair( 1, 0, 0, 6 ); 
			GL_DrawColoredStereoLinePair( 0, 1, 0, 8 ); 
			GL_DrawColoredStereoLinePair( 1, 1, 0, 10); 
			GL_DrawColoredStereoLinePair( 1, 1, 0, 12); 
			GL_DrawColoredStereoLinePair( 0, 1, 0, 14); 
		qglEnd(); 
		 
		GLimp_EndFrame(); 
	} 
}

#endif

/* 
==================== 
R_SetLightLevel 
 
==================== 
*/
 
void R_SetLightLevel (void) 
{ 
	vec3_t	shadelight; 
 
	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL) 
		return; 
 
	// save off light value for server to look at (BIG HACK!) 
 
	R_LightPoint (r_newrefdef.vieworg, shadelight); 
 
	// pick the greatest component, which should be the same 
	// as the mono value returned by software 
	if (shadelight[0] > shadelight[1]) 
	{ 
		if (shadelight[0] > shadelight[2]) 
			r_lightlevel->value = 150*shadelight[0];
 
		else 
			r_lightlevel->value = 150*shadelight[2]; 
	}
 
	else 
	{ 
		if (shadelight[1] > shadelight[2]) 
			r_lightlevel->value = 150*shadelight[1];
 
		else 
			r_lightlevel->value = 150*shadelight[2]; 
	} 
 
} 
 
/* 
@@@@@@@@@@@@@@@@@@@@@ 
R_RenderFrame 
 
@@@@@@@@@@@@@@@@@@@@@ 
*/

DLLFUNC void R_RenderFrame (refdef_t *fd) 
{ 
	R_RenderView( fd ); 
	R_SetLightLevel (); 
	R_SetGL2D (); 
} 
 
void R_Register( void ) 
{ 
	r_lefthand = ri.Cvar_Get( "hand", "0", CVAR_USERINFO | CVAR_ARCHIVE ); 
	r_norefresh = ri.Cvar_Get ("r_norefresh", "0", 0); 
	r_fullbright = ri.Cvar_Get ("r_fullbright", "0", 0); 
	r_drawentities = ri.Cvar_Get ("r_drawentities", "1", 0); 
	r_drawworld = ri.Cvar_Get ("r_drawworld", "1", 0); 
	r_novis = ri.Cvar_Get ("r_novis", "0", 0); 
	r_nocull = ri.Cvar_Get ("r_nocull", "0", 0); 
	r_lerpmodels = ri.Cvar_Get ("r_lerpmodels", "1", CVAR_ARCHIVE); //surgeon: added CVAR_ARCHIVE 
	r_speeds = ri.Cvar_Get ("r_speeds", "0", 0); 
 
	r_lightlevel = ri.Cvar_Get ("r_lightlevel", "0", 0); 

	#ifdef AMIGAOS

	gl_keeptjunctions =  ri.Cvar_Get( "gl_keeptjunctions", "1", CVAR_ARCHIVE ); 
	gl_smoothmodels =  ri.Cvar_Get( "gl_smoothmodels", "1", CVAR_ARCHIVE ); 
	gl_affinemodels =  ri.Cvar_Get( "gl_affinemodels", "0", CVAR_ARCHIVE );

	//gl_ext_clip_volume_hint =  ri.Cvar_Get( "gl_ext_clip_volume_hint", "0", CVAR_ARCHIVE ); // Cowcat
	r_vertexlighting =  ri.Cvar_Get( "r_vertexlighting", "0", CVAR_ARCHIVE ); 
	gl_polycull =  ri.Cvar_Get( "gl_polycull", "1", CVAR_ARCHIVE ); 
	//gl_multitexturesort =  ri.Cvar_Get( "gl_multitexturesort", "0", CVAR_ARCHIVE );
 
	#endif

	gl_nosubimage = ri.Cvar_Get( "gl_nosubimage", "0", 0 ); 
	gl_allow_software = ri.Cvar_Get( "gl_allow_software", "0", 0 ); 
 
	gl_particle_min_size = ri.Cvar_Get( "gl_particle_min_size", "2", CVAR_ARCHIVE ); 
	gl_particle_max_size = ri.Cvar_Get( "gl_particle_max_size", "40", CVAR_ARCHIVE ); 
	gl_particle_size = ri.Cvar_Get( "gl_particle_size", "40", CVAR_ARCHIVE ); 
	gl_particle_att_a = ri.Cvar_Get( "gl_particle_att_a", "0.01", CVAR_ARCHIVE ); 
	gl_particle_att_b = ri.Cvar_Get( "gl_particle_att_b", "0.0", CVAR_ARCHIVE ); 
	gl_particle_att_c = ri.Cvar_Get( "gl_particle_att_c", "0.01", CVAR_ARCHIVE ); 
 
	gl_modulate = ri.Cvar_Get ("gl_modulate", "1", CVAR_ARCHIVE ); 
	gl_log = ri.Cvar_Get( "gl_log", "0", 0 ); 
	gl_bitdepth = ri.Cvar_Get( "gl_bitdepth", "0", 0 ); 
	gl_mode = ri.Cvar_Get( "gl_mode", "3", CVAR_ARCHIVE ); 
	gl_lightmap = ri.Cvar_Get ("gl_lightmap", "0", 0); 
	gl_shadows = ri.Cvar_Get ("gl_shadows", "0", CVAR_ARCHIVE ); 
	gl_dynamic = ri.Cvar_Get ("gl_dynamic", "1", 0); 
	gl_nobind = ri.Cvar_Get ("gl_nobind", "0", 0); 
	gl_round_down = ri.Cvar_Get ("gl_round_down", "1", 0); 
	gl_picmip = ri.Cvar_Get ("gl_picmip", "0", 0); 
	gl_skymip = ri.Cvar_Get ("gl_skymip", "0", 0); 
	gl_showtris = ri.Cvar_Get ("gl_showtris", "0", 0); 
	gl_ztrick = ri.Cvar_Get ("gl_ztrick", "0", 0); 
	gl_finish = ri.Cvar_Get ("gl_finish", "0", CVAR_ARCHIVE); 
	gl_clear = ri.Cvar_Get ("gl_clear", "0", 0); 
	gl_cull = ri.Cvar_Get ("gl_cull", "1", 0); 
	gl_polyblend = ri.Cvar_Get ("gl_polyblend", "1", 0);
	gl_flashblend = ri.Cvar_Get ("gl_flashblend", "0", CVAR_ARCHIVE); //surgeon: added CVAR_ARCHIVE flag
 	gl_playermip = ri.Cvar_Get ("gl_playermip", "0", 0);
	gl_monolightmap = ri.Cvar_Get( "gl_monolightmap", "0", 0 ); 
	gl_driver = ri.Cvar_Get( "gl_driver", "opengl32", CVAR_ARCHIVE );

	#ifdef AMIGAOS

	gl_texturemode = ri.Cvar_Get( "gl_texturemode", "GL_LINEAR", CVAR_ARCHIVE );

	#else

	gl_texturemode = ri.Cvar_Get( "gl_texturemode", "GL_LINEAR_MIPMAP_NEAREST", CVAR_ARCHIVE );
 
	#endif
 
	gl_texturealphamode = ri.Cvar_Get( "gl_texturealphamode", "default", CVAR_ARCHIVE ); 
	gl_texturesolidmode = ri.Cvar_Get( "gl_texturesolidmode", "default", CVAR_ARCHIVE ); 
	gl_lockpvs = ri.Cvar_Get( "gl_lockpvs", "0", 0 ); 
 
	gl_vertex_arrays = ri.Cvar_Get( "gl_vertex_arrays", "1", CVAR_ARCHIVE ); // now 1 Cowcat
 
	gl_ext_swapinterval = ri.Cvar_Get( "gl_ext_swapinterval", "1", CVAR_ARCHIVE ); 
	gl_ext_palettedtexture = ri.Cvar_Get( "gl_ext_palettedtexture", "1", CVAR_ARCHIVE );
	gl_ext_multitexture = ri.Cvar_Get( "gl_ext_multitexture", "0", CVAR_ARCHIVE ); // now 0 Cowcat
	gl_ext_pointparameters = ri.Cvar_Get( "gl_ext_pointparameters", "1", CVAR_ARCHIVE );
	gl_ext_compiled_vertex_array = ri.Cvar_Get( "gl_ext_compiled_vertex_array", "1", CVAR_ARCHIVE ); 
 
	gl_drawbuffer = ri.Cvar_Get( "gl_drawbuffer", "GL_BACK", 0 ); 
	gl_swapinterval = ri.Cvar_Get( "gl_swapinterval", "1", CVAR_ARCHIVE ); 
 
	gl_saturatelighting = ri.Cvar_Get( "gl_saturatelighting", "0", 0 ); 
 
	gl_3dlabs_broken = ri.Cvar_Get( "gl_3dlabs_broken", "1", CVAR_ARCHIVE );

	// fog stuff from quake tutorial - Cowcat
	gl_fog = ri.Cvar_Get( "gl_fog","0",CVAR_ARCHIVE );
	gl_fogstart = ri.Cvar_Get( "gl_fogstart","50.0", CVAR_ARCHIVE);
	gl_fogend = ri.Cvar_Get( "gl_fogend","800.0", CVAR_ARCHIVE);
	gl_fogred = ri.Cvar_Get( "gl_fogred","0.6", CVAR_ARCHIVE);
	gl_foggreen = ri.Cvar_Get( "gl_foggreen","0.5", CVAR_ARCHIVE);
	gl_fogblue = ri.Cvar_Get( "gl_fogblue","0.4", CVAR_ARCHIVE);

	gl_showbbox = ri.Cvar_Get( "gl_showbbox","0", 0);
	gl_nolerp_list = ri.Cvar_Get ("gl_nolerp_list", "pics/conchars.pcx pics/ch1.pcx pics/ch2.pcx pics/ch3.pcx" , 0); // new Yamagi

	//
 
	vid_fullscreen = ri.Cvar_Get( "vid_fullscreen", "0", CVAR_ARCHIVE ); 
	vid_gamma = ri.Cvar_Get( "vid_gamma", "1.0", CVAR_ARCHIVE ); 
	vid_ref = ri.Cvar_Get( "vid_ref", "soft", CVAR_ARCHIVE ); 
	ri.Cmd_AddCommand( "imagelist", GL_ImageList_f ); 
	ri.Cmd_AddCommand( "screenshot", GL_ScreenShot_f ); 
	ri.Cmd_AddCommand( "modellist", Mod_Modellist_f ); 
	ri.Cmd_AddCommand( "gl_strings", GL_Strings_f ); 
} 
 
/* 
================== 
R_SetMode 
================== 
*/

qboolean R_SetMode (void) 
{ 
	rserr_t 	err; 
	qboolean 	fullscreen; 
 
	#ifndef AMIGAOS

	if ( vid_fullscreen->modified && !gl_config.allow_cds ) 
	{ 
		ri.Con_Printf( PRINT_ALL, "R_SetMode() - CDS not allowed with this driver\n" ); 
		ri.Cvar_SetValue( "vid_fullscreen", !vid_fullscreen->value ); 
		vid_fullscreen->modified = false; 
	} 

	#endif 
 
	fullscreen = vid_fullscreen->value; 
 
	vid_fullscreen->modified = false; 
	gl_mode->modified = false; 
 
	if ( ( err = GLimp_SetMode( &vid.width, &vid.height, gl_mode->value, fullscreen ) ) == rserr_ok ) 
	{ 
		gl_state.prev_mode = gl_mode->value; 
	} 

	else 
	{ 
		if ( err == rserr_invalid_fullscreen ) 
		{ 
			ri.Cvar_SetValue( "vid_fullscreen", 0); 
			vid_fullscreen->modified = false; 
			ri.Con_Printf( PRINT_ALL, "ref_gl::R_SetMode() - fullscreen unavailable in this mode\n" );
 
			if ( ( err = GLimp_SetMode( &vid.width, &vid.height, gl_mode->value, false ) ) == rserr_ok ) 
				return true; 
		} 

		else if ( err == rserr_invalid_mode ) 
		{ 
			ri.Cvar_SetValue( "gl_mode", gl_state.prev_mode ); 
			gl_mode->modified = false; 
			ri.Con_Printf( PRINT_ALL, "ref_gl::R_SetMode() - invalid mode\n" ); 
		} 
 
		// try setting it back to something safe 
		if ( ( err = GLimp_SetMode( &vid.width, &vid.height, gl_state.prev_mode, false ) ) != rserr_ok ) 
		{ 
			ri.Con_Printf( PRINT_ALL, "ref_gl::R_SetMode() - could not revert to safe mode\n" ); 
			return false; 
		} 
	} 

	return true; 
} 
 

/* 
=============== 
R_Init 
=============== 
*/
 
DLLFUNC qboolean R_Init( void *hinstance, void *hWnd ) 
{        
	char 	renderer_buffer[1000]; 
	char 	vendor_buffer[1000]; 
	int     err; 
	int	j; 

	extern float 	r_turbsin[256]; 
 
	for ( j = 0; j < 256; j++ ) 
	{ 
		r_turbsin[j] *= 0.5; 
	} 
 
	ri.Con_Printf (PRINT_ALL, "ref_gl version: "REF_VERSION"\n"); 
 
	Draw_GetPalette (); 
 
	R_Register(); 
 
	// initialize our QGL dynamic bindings 
	if ( !QGL_Init( gl_driver->string ) ) 
	{ 
		QGL_Shutdown(); 
		ri.Con_Printf (PRINT_ALL, "ref_gl::R_Init() - could not load \"%s\"\n", gl_driver->string ); 
		return -1; 
	} 
 
	// initialize OS-specific parts of OpenGL 
	if ( !GLimp_Init( hinstance, hWnd ) ) 
	{ 
		QGL_Shutdown(); 
		return -1; 
	} 
 
	// set our "safe" modes 
	gl_state.prev_mode = 3; 
 
	// create the window and set up the context 
	if ( !R_SetMode () ) 
	{ 
		QGL_Shutdown(); 
		ri.Con_Printf (PRINT_ALL, "ref_gl::R_Init() - could not R_SetMode()\n" ); 
		return -1; 
	} 
 
	ri.Vid_MenuInit(); 
 
	/* 
	** get our various GL strings 
	*/

	gl_config.vendor_string = qglGetString (GL_VENDOR); 
	ri.Con_Printf (PRINT_ALL, "GL_VENDOR: %s\n", gl_config.vendor_string ); 
	gl_config.renderer_string = qglGetString (GL_RENDERER); 
	ri.Con_Printf (PRINT_ALL, "GL_RENDERER: %s\n", gl_config.renderer_string ); 
	gl_config.version_string = qglGetString (GL_VERSION); 
	ri.Con_Printf (PRINT_ALL, "GL_VERSION: %s\n", gl_config.version_string ); 
	gl_config.extensions_string = qglGetString (GL_EXTENSIONS); 
	ri.Con_Printf (PRINT_ALL, "GL_EXTENSIONS: %s\n", gl_config.extensions_string ); 
 
	strcpy( renderer_buffer, gl_config.renderer_string ); 
	strlwr( renderer_buffer ); 
 
	strcpy( vendor_buffer, gl_config.vendor_string ); 
	strlwr( vendor_buffer ); 
 
	if ( strstr( renderer_buffer, "voodoo" ) ) 
	{ 
		if ( !strstr( renderer_buffer, "rush" ) ) 
			gl_config.renderer = GL_RENDERER_VOODOO;

		else 
			gl_config.renderer = GL_RENDERER_VOODOO_RUSH; 
	}
 
	else if ( strstr( vendor_buffer, "sgi" ) ) 
		gl_config.renderer = GL_RENDERER_SGI; 

	else if ( strstr( renderer_buffer, "permedia" ) ) 
		gl_config.renderer = GL_RENDERER_PERMEDIA2; 

	else if ( strstr( renderer_buffer, "glint" ) ) 
		gl_config.renderer = GL_RENDERER_GLINT_MX; 

	else if ( strstr( renderer_buffer, "glzicd" ) ) 
		gl_config.renderer = GL_RENDERER_REALIZM; 

	else if ( strstr( renderer_buffer, "gdi" ) ) 
		gl_config.renderer = GL_RENDERER_MCD; 

	else if ( strstr( renderer_buffer, "pcx2" ) ) 
		gl_config.renderer = GL_RENDERER_PCX2; 

	else if ( strstr( renderer_buffer, "verite" ) ) 
		gl_config.renderer = GL_RENDERER_RENDITION; 

	else 
		gl_config.renderer = GL_RENDERER_OTHER; 
 
	if ( toupper( gl_monolightmap->string[1] ) != 'F' ) 
	{ 
		if ( gl_config.renderer == GL_RENDERER_PERMEDIA2 ) 
		{ 
			ri.Cvar_Set( "gl_monolightmap", "A" ); 
			ri.Con_Printf( PRINT_ALL, "...using gl_monolightmap 'a'\n" ); 
		} 

		else if ( gl_config.renderer & GL_RENDERER_POWERVR )  
		{ 
			ri.Cvar_Set( "gl_monolightmap", "0" ); 
		} 

		else 
		{ 
			ri.Cvar_Set( "gl_monolightmap", "0" ); 
		} 
	} 
 
	// power vr can't have anything stay in the framebuffer, so 
	// the screen needs to redraw the tiled background every frame 
	if ( gl_config.renderer & GL_RENDERER_POWERVR )  
	{ 
		ri.Cvar_Set( "scr_drawall", "1" ); 
	} 

	else 
	{ 
		ri.Cvar_Set( "scr_drawall", "0" ); 
	} 
 
#ifdef __linux__ 
	ri.Cvar_SetValue( "gl_finish", 1 ); 
#endif 
 
	// MCD has buffering issues 
	if ( gl_config.renderer == GL_RENDERER_MCD ) 
	{ 
		ri.Cvar_SetValue( "gl_finish", 1 ); 
	} 
 
	if ( gl_config.renderer & GL_RENDERER_3DLABS ) 
	{ 
		if ( gl_3dlabs_broken->value ) 
			gl_config.allow_cds = false; 

		else 
			gl_config.allow_cds = true; 
	} 

	else 
	{ 
		gl_config.allow_cds = true; 
	} 
 
	if ( gl_config.allow_cds ) 
		ri.Con_Printf( PRINT_ALL, "...allowing CDS\n" ); 

	else 
		ri.Con_Printf( PRINT_ALL, "...disabling CDS\n" ); 
 
	/* 
	** grab extensions 
	*/ 
	if ( strstr( gl_config.extensions_string, "GL_EXT_compiled_vertex_array" ) ||  
		 strstr( gl_config.extensions_string, "GL_SGI_compiled_vertex_array" ) ) 
	{ 
		ri.Con_Printf( PRINT_ALL, "...enabling GL_EXT_compiled_vertex_array\n" ); 
		qglLockArraysEXT = ( void * ) qwglGetProcAddress( "glLockArraysEXT" ); 
		qglUnlockArraysEXT = ( void * ) qwglGetProcAddress( "glUnlockArraysEXT" ); 
	} 

	else 
	{ 
		ri.Con_Printf( PRINT_ALL, "...GL_EXT_compiled_vertex_array not found\n" ); 
	} 
 
#ifdef _WIN32 
	if ( strstr( gl_config.extensions_string, "WGL_EXT_swap_control" ) ) 
	{ 
		qwglSwapIntervalEXT = ( BOOL (WINAPI *)(int)) qwglGetProcAddress( "wglSwapIntervalEXT" ); 
		ri.Con_Printf( PRINT_ALL, "...enabling WGL_EXT_swap_control\n" ); 
	} 
	else 
	{ 
		ri.Con_Printf( PRINT_ALL, "...WGL_EXT_swap_control not found\n" ); 
	} 
#endif 
 
	if ( strstr( gl_config.extensions_string, "GL_EXT_point_parameters" ) ) 
	{ 
		if ( gl_ext_pointparameters->value ) 
		{ 
			qglPointParameterfEXT = ( void (APIENTRY *)( GLenum, GLfloat ) ) qwglGetProcAddress( "glPointParameterfEXT" ); 
			qglPointParameterfvEXT = ( void (APIENTRY *)( GLenum, const GLfloat * ) ) qwglGetProcAddress( "glPointParameterfvEXT" ); 
			ri.Con_Printf( PRINT_ALL, "...using GL_EXT_point_parameters\n" ); 
		}
 
		else 
		{ 
			ri.Con_Printf( PRINT_ALL, "...ignoring GL_EXT_point_parameters\n" ); 
		} 
	} 

	else 
	{ 
		ri.Con_Printf( PRINT_ALL, "...GL_EXT_point_parameters not found\n" ); 
	} 
 
#ifdef __linux__ 
	if ( strstr( gl_config.extensions_string, "3DFX_set_global_palette" )) 
	{ 
		if ( gl_ext_palettedtexture->value ) 
		{ 
			ri.Con_Printf( PRINT_ALL, "...using 3DFX_set_global_palette\n" ); 
			qgl3DfxSetPaletteEXT = ( void ( APIENTRY * ) (GLuint *) )qwglGetProcAddress( "gl3DfxSetPaletteEXT" ); 
			qglColorTableEXT = Fake_glColorTableEXT; 
		} 

		else 
		{ 
			ri.Con_Printf( PRINT_ALL, "...ignoring 3DFX_set_global_palette\n" ); 
		} 
	}
 
	else 
	{ 
		ri.Con_Printf( PRINT_ALL, "...3DFX_set_global_palette not found\n" ); 
	} 
#endif 
 
	if ( !qglColorTableEXT && 
		strstr( gl_config.extensions_string, "GL_EXT_paletted_texture" ) &&  
		 strstr( gl_config.extensions_string, "GL_EXT_shared_texture_palette" ) ) 
	{ 
		if ( gl_ext_palettedtexture->value ) 
		{ 
			ri.Con_Printf( PRINT_ALL, "...using GL_EXT_shared_texture_palette\n" ); 
			qglColorTableEXT = ( void ( APIENTRY * ) ( int, int, int, int, int, const void * ) ) qwglGetProcAddress( "glColorTableEXT" ); 
		} 

		else 
		{ 
			ri.Con_Printf( PRINT_ALL, "...ignoring GL_EXT_shared_texture_palette\n" ); 
		} 
	}
 
	else 
	{ 
		ri.Con_Printf( PRINT_ALL, "...GL_EXT_shared_texture_palette not found\n" ); 
	} 

#ifndef AMIGAOS

	if ( strstr( gl_config.extensions_string, "GL_ARB_multitexture" ) ) 
	{ 
		if ( gl_ext_multitexture->value ) 
		{ 
			ri.Con_Printf( PRINT_ALL, "...using GL_ARB_multitexture\n" ); 
			qglMultiTexCoord2fARB = ( void * ) qwglGetProcAddress( "glMultiTexCoord2fARB" ); 
			qglActiveTextureARB = ( void * ) qwglGetProcAddress( "glActiveTextureARB" ); 
			qglClientActiveTextureARB = ( void * ) qwglGetProcAddress( "glClientActiveTextureARB" ); 
			arb_multitexture = true;
		} 

		else 
		{ 
			ri.Con_Printf( PRINT_ALL, "...ignoring GL_ARB_multitexture\n" ); 
		} 
	} 

	else 
	{ 
		ri.Con_Printf( PRINT_ALL, "...GL_ARB_multitexture not found\n" ); 
	} 
#else

//	#if 0 //disabled in MiniGL

	if ( strstr( gl_config.extensions_string, "GL_MGL_ARB_multitexture" ) ) 
	{ 
		if ( gl_ext_multitexture->value ) 
		{ 
			ri.Con_Printf( PRINT_ALL, "...using GL_MGL_ARB_multitexture\n" ); 

			mgl_arb_multitexture = true;
		}
 
		else 
		{ 
			ri.Con_Printf( PRINT_ALL, "...ignoring GL_MGL_ARB_multitexture\n" ); 
		} 
	} 

	else 
	{ 
		ri.Con_Printf( PRINT_ALL, "...GL_MGL_ARB_multitexture not found\n" ); 
	}

//	#endif 

#endif
 
	err = qglGetError(); 

	if ( err != GL_NO_ERROR ) 
		ri.Con_Printf (PRINT_ALL, "After basic init: glGetError() = 0x%x\n", err); 
 
	GL_SetDefaultState(); 
 
	/* 
	** draw our stereo patterns 
	*/ 

#ifdef STEREO // commented out until H3D pays us the money they owe us 
	GL_DrawStereoPattern(); 
#endif 
 
	err = qglGetError();
 
	if ( err != GL_NO_ERROR ) 
		ri.Con_Printf (PRINT_ALL, "After GL_SetDefaultState: glGetError() = 0x%x\n", err); 
	 
	GL_InitImages (); 
	err = qglGetError(); 

	if ( err != GL_NO_ERROR ) 
		ri.Con_Printf (PRINT_ALL, "After GL_InitImages: glGetError() = 0x%x\n", err); 
	 
	Mod_Init (); 
	err = qglGetError(); 

	if ( err != GL_NO_ERROR ) 
		ri.Con_Printf (PRINT_ALL, "After Mod_Init: glGetError() = 0x%x\n", err); 
 
	R_InitParticleTexture (); 
	err = qglGetError(); 

	if ( err != GL_NO_ERROR ) 
		ri.Con_Printf (PRINT_ALL, "After R_InitParticleTexture: glGetError() = 0x%x\n", err); 
 
	Draw_InitLocal (); 
	err = qglGetError(); 

	if ( err != GL_NO_ERROR ) 
		ri.Con_Printf (PRINT_ALL, "After Draw_InitLocal: glGetError() = 0x%x\n", err);

	return true;
} 
 
/* 
=============== 
R_Shutdown 
=============== 
*/ 

DLLFUNC void R_Shutdown (void) 
{        
	ri.Cmd_RemoveCommand ("modellist"); 
	ri.Cmd_RemoveCommand ("screenshot"); 
	ri.Cmd_RemoveCommand ("imagelist"); 
	ri.Cmd_RemoveCommand ("gl_strings"); 
 
	Mod_FreeAll (); 
 
	GL_ShutdownImages (); 
 
	/* 
	** shut down OS specific OpenGL stuff like contexts, etc. 
	*/ 
	GLimp_Shutdown(); 
 
	/* 
	** shutdown our QGL subsystem 
	*/ 
	QGL_Shutdown(); 
} 
 
/* 
@@@@@@@@@@@@@@@@@@@@@ 
R_BeginFrame 
@@@@@@@@@@@@@@@@@@@@@ 
*/

#ifdef STEREO 
DLLFUNC void R_BeginFrame( float camera_separation ) 
#else
DLLFUNC void R_BeginFrame( void ) 
#endif
{ 
 	#ifdef STEREO
	gl_state.camera_separation = camera_separation;
	#endif 
 
	/* 
	** change modes if necessary 
	*/ 
	if ( gl_mode->modified || vid_fullscreen->modified ) 
	{
	       // FIXME: only restart if CDS is required 
		cvar_t  *ref; 
 
		ref = ri.Cvar_Get ("vid_ref", "gl", 0); 
		ref->modified = true; 
	} 
 
	if ( gl_log->modified ) 
	{ 
		GLimp_EnableLogging( gl_log->value ); 
		gl_log->modified = false; 
	} 
 
	if ( gl_log->value ) 
	{ 
		GLimp_LogNewFrame(); 
	} 
 
	/* 
	** update 3Dfx gamma -- it is expected that a user will do a vid_restart 
	** after tweaking this value 
	*/ 
	if ( vid_gamma->modified ) 
	{ 
		vid_gamma->modified = false; 
 
		if ( gl_config.renderer & ( GL_RENDERER_VOODOO ) ) 
		{
			#ifndef AMIGAOS

			char envbuffer[1024]; 
			float g; 

			g = 2.00 * ( 0.8 - ( vid_gamma->value - 0.5 ) ) + 1.0F; 
			Com_sprintf( envbuffer, sizeof(envbuffer), "SSTV2_GAMMA=%f", g ); 
			putenv( envbuffer ); 
			Com_sprintf( envbuffer, sizeof(envbuffer), "SST_GAMMA=%f", g ); 
			putenv( envbuffer );

			#endif 
		} 
	} 
 
	#ifdef STEREO
	GLimp_BeginFrame( camera_separation ); 
	#else
	GLimp_BeginFrame();
	#endif
 
	/* 
	** go into 2D mode 
	*/
 
	#if 0

	qglViewport (0,0, vid.width, vid.height); 
	qglMatrixMode(GL_PROJECTION); 
    	qglLoadIdentity (); 
	qglOrtho  (0, vid.width, vid.height, 0, -99999, 99999); 
	qglMatrixMode(GL_MODELVIEW); 
    	qglLoadIdentity (); 
	qglDisable (GL_DEPTH_TEST); 
	qglDisable (GL_CULL_FACE); 
	qglDisable (GL_BLEND); 
	qglEnable (GL_ALPHA_TEST); 
	qglColor4f (1,1,1,1); 

	#else

	R_SetGL2D();

	#endif
 
	/* 
	** draw buffer stuff 
	*/ 

	if ( gl_drawbuffer->modified ) 
	{ 
		gl_drawbuffer->modified = false; 

		#ifdef STEREO 
		if ( gl_state.camera_separation == 0 || !gl_state.stereo_enabled ) 
		#endif

		{ 
			if ( Q_strcasecmp( gl_drawbuffer->string, "GL_FRONT" ) == 0 ) 
				qglDrawBuffer( GL_FRONT ); 

			else 
				qglDrawBuffer( GL_BACK ); 
		} 
	} 
 
	/* 
	** texturemode stuff 
	*/ 
	if ( gl_texturemode->modified ) 
	{ 
		GL_TextureMode( gl_texturemode->string ); 
		gl_texturemode->modified = false; 
	} 
 
	if ( gl_texturealphamode->modified ) 
	{ 
		GL_TextureAlphaMode( gl_texturealphamode->string ); 
		gl_texturealphamode->modified = false; 
	} 
 
	if ( gl_texturesolidmode->modified ) 
	{ 
		GL_TextureSolidMode( gl_texturesolidmode->string ); 
		gl_texturesolidmode->modified = false; 
	} 
 
	/* 
	** swapinterval stuff 
	*/
 
	GL_UpdateSwapInterval(); 
 
	// 
	// clear screen if desired 
	// 
	R_Clear (); 
} 
 
/* 
============= 
R_SetPalette 
============= 
*/ 

unsigned r_rawpalette[256]; 
 
DLLFUNC void R_SetPalette ( const unsigned char *palette) 
{ 
	int	i; 
 
	byte *rp = ( byte * ) r_rawpalette; 
 
	if ( palette ) 
	{ 
		for ( i = 0; i < 256; i++ ) 
		{ 
			rp[i*4+0] = palette[i*3+0]; 
			rp[i*4+1] = palette[i*3+1]; 
			rp[i*4+2] = palette[i*3+2]; 
			rp[i*4+3] = 0xff; 
		} 
	} 

	else 
	{ 
		for ( i = 0; i < 256; i++ ) 
		{
			// littlelong needed ? - Cowcat
			rp[i*4+0] = LittleLong(d_8to24table[i]) & 0xff; 
			rp[i*4+1] = LittleLong( (d_8to24table[i]) >> 8 ) & 0xff; 
			rp[i*4+2] = LittleLong( (d_8to24table[i]) >> 16 ) & 0xff; 
			rp[i*4+3] = 0xff; 
		} 
	} 

	GL_SetTexturePalette( r_rawpalette ); 
 
	qglClearColor (0,0,0,0); 
	qglClear (GL_COLOR_BUFFER_BIT);
 
	//surgeon: was (1.0 0.0, 0.5, 0.5) yuck!
	qglClearColor( 0.0, 0.0, 0.0, 1.0 );
} 
 
/* 
** R_DrawBeam 
*/

void R_DrawBeam( entity_t *e ) 
{ 
	#define NUM_BEAM_SEGS 6 
 
	int     i; 
	float 	r, g, b; 
 
	vec3_t perpvec; 
	vec3_t direction, normalized_direction; 
	vec3_t start_points[NUM_BEAM_SEGS], end_points[NUM_BEAM_SEGS]; 
	vec3_t oldorigin, origin; 

	if(fog)
		qglDisable(GL_FOG); // Cowcat
 
	oldorigin[0] = e->oldorigin[0]; 
	oldorigin[1] = e->oldorigin[1]; 
	oldorigin[2] = e->oldorigin[2]; 
 
	origin[0] = e->origin[0]; 
	origin[1] = e->origin[1]; 
	origin[2] = e->origin[2]; 
 
	normalized_direction[0] = direction[0] = oldorigin[0] - origin[0]; 
	normalized_direction[1] = direction[1] = oldorigin[1] - origin[1]; 
	normalized_direction[2] = direction[2] = oldorigin[2] - origin[2]; 
 
	if ( VectorNormalize( normalized_direction ) == 0 ) 
		return; 
 
	PerpendicularVector( perpvec, normalized_direction ); 
	VectorScale( perpvec, e->frame / 2, perpvec ); 
 
	for ( i = 0; i < 6; i++ ) 
	{ 
		RotatePointAroundVector( start_points[i], normalized_direction, perpvec, (360.0/NUM_BEAM_SEGS)*i ); 
		VectorAdd( start_points[i], origin, start_points[i] ); 
		VectorAdd( start_points[i], direction, end_points[i] ); 
	} 
 
	qglDisable( GL_TEXTURE_2D ); 

	#ifndef AMIGAOS //done globally for all translucent ents

	qglEnable( GL_BLEND ); 
	qglDepthMask( GL_FALSE ); 

	#endif
 
	// littlelong needed ? - Cowcat
	r = ( LittleLong(d_8to24table[e->skinnum & 0xFF] )) & 0xFF; 
	g = ( LittleLong(d_8to24table[e->skinnum & 0xFF] ) >> 8 ) & 0xFF; 
	b = ( LittleLong(d_8to24table[e->skinnum & 0xFF] ) >> 16 ) & 0xFF; 
 
	r *= 1/255.0F; 
	g *= 1/255.0F; 
	b *= 1/255.0F; 
 
	qglColor4f( r, g, b, e->alpha ); 
 
	qglBegin( GL_TRIANGLE_STRIP ); 

	for ( i = 0; i < NUM_BEAM_SEGS; i++ ) 
	{ 
		qglVertex3fv( start_points[i] ); 
		qglVertex3fv( end_points[i] ); 
		qglVertex3fv( start_points[(i+1)%NUM_BEAM_SEGS] ); 
		qglVertex3fv( end_points[(i+1)%NUM_BEAM_SEGS] ); 
	} 
	qglEnd(); 
 
	qglEnable( GL_TEXTURE_2D ); 

	#ifndef AMIGAOS //done globally for all translucent ents

	qglDisable( GL_BLEND );
	qglDepthMask( GL_TRUE ); 

	#endif

	if(fog)
		qglEnable(GL_FOG); //Cowcat
} 
 
//=================================================================== 
 
 
DLLFUNC void    R_BeginRegistration (char *map); 
DLLFUNC struct 	model_s  *R_RegisterModel (char *name); 
DLLFUNC struct 	image_s  *R_RegisterSkin (char *name); 
DLLFUNC void 	R_SetSky (char *name, float rotate, vec3_t axis); 
DLLFUNC void    R_EndRegistration (void); 
 
DLLFUNC void    R_RenderFrame (refdef_t *fd); 
 
DLLFUNC struct 	image_s  *Draw_FindPic (char *name); 
 
DLLFUNC void    Draw_Pic (int x, int y, char *name); 
DLLFUNC void    Draw_Char (int x, int y, int c); 
DLLFUNC void    Draw_TileClear (int x, int y, int w, int h, char *name); 
DLLFUNC void    Draw_Fill (int x, int y, int w, int h, int c); 
DLLFUNC void    Draw_FadeScreen (void); 

DLLFUNC void	GLimp_EmptyCursor (void);  // Cowcat
DLLFUNC void	GLimp_EnableCursor (void); // Cowcat
 
/* 
@@@@@@@@@@@@@@@@@@@@@ 
GetRefAPI 
 
@@@@@@@@@@@@@@@@@@@@@ 
*/
 
DLLFUNC refexport_t GetRefAPI (refimport_t rimp ) 
{ 
	refexport_t     re; 
 
	ri = rimp; 
 
	re.api_version = API_VERSION; 
 
	re.BeginRegistration = R_BeginRegistration; 
	re.RegisterModel = R_RegisterModel; 
	re.RegisterSkin = R_RegisterSkin; 
	re.RegisterPic = Draw_FindPic; 
	re.SetSky = R_SetSky; 
	re.EndRegistration = R_EndRegistration; 
 
	re.RenderFrame = R_RenderFrame; 
 
	re.DrawGetPicSize = Draw_GetPicSize; 
	re.DrawPic = Draw_Pic; 
	re.DrawStretchPic = Draw_StretchPic; 
	re.DrawChar = Draw_Char; 
	re.DrawTileClear = Draw_TileClear; 
	re.DrawFill = Draw_Fill; 
	re.DrawFadeScreen= Draw_FadeScreen; 
 
	re.DrawStretchRaw = Draw_StretchRaw; 
 
	re.Init = R_Init; 
	re.Shutdown = R_Shutdown; 
 
	re.CinematicSetPalette = R_SetPalette; 
	re.BeginFrame = R_BeginFrame; 
	re.EndFrame = GLimp_EndFrame; 
 
	re.AppActivate = GLimp_AppActivate; 
 
	re.EmptyCursor = GLimp_EmptyCursor;  // Cowcat
	re.EnableCursor = GLimp_EnableCursor; // Cowcat

	Swap_Init (); 
 
	return re; 
} 
 
 
#ifndef REF_HARD_LINKED 

// this is only here so the functions in q_shared.c and q_shwin.c can link 
void Sys_Error (char *error, ...) 
{ 
	va_list         argptr; 
	char            text[1024]; 
 
	va_start 	(argptr, error); 
	vsprintf 	(text, error, argptr); 
	va_end 		(argptr); 
 
	ri.Sys_Error (ERR_FATAL, "%s", text); 
} 
 
void Com_Printf (char *fmt, ...) 
{ 
	va_list         argptr; 
	char            text[1024]; 
 
	va_start 	(argptr, fmt); 
	vsprintf 	(text, fmt, argptr); 
	va_end 		(argptr); 
 
	ri.Con_Printf (PRINT_ALL, "%s", text); 
} 
 
#endif

