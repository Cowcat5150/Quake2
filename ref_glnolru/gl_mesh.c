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
// gl_mesh.c: triangle model functions 
 
#include "gl_local.h" 
 
/* 
============================================================= 
 
  ALIAS MODELS 
 
============================================================= 
*/ 
 
#define NUMVERTEXNORMALS 	162
#define SHADEDOT_QUANT		16
 
#if 0 //surgeon: moved and declared const
 
float	r_avertexnormals[NUMVERTEXNORMALS][3] = { 
#include "anorms.h" 
};

// precalculated dot products for quantized angles 

float	r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "anormtab.h" 
; 
float	*shadedots = r_avertexnormal_dots[0];  

#endif

typedef float vec4_t[4]; 

typedef struct varray_s
{
	GLfloat data[3];
	GLuint	color;

	/*
	** 3rd data slot is used for MiniGL w-coords with
	** vertexarrays
	** in standard mode, data may be used for preparing
	** rgb colors
	*/

} varray_t;

extern cvar_t	*gl_smoothmodels;

//#define AMIGA_VOLATILE_ARRAYS 1  // Not on minigl aminet source Cowcat

/* experimental feature */
/* this invalidates shadow-data if using vertexarrays, so we have to copy the data before the arrays are locked */

#ifndef AMIGA_VOLATILE_ARRAYS

	#define LERPSIZE 4 

	static	 vec4_t	 s_lerped[MAX_VERTS];
	static varray_t	 vArray[MAX_VERTS];

#else

	#define LERPSIZE  MGL_ARRAY_STRIDE_4

	#define S_COORD	  (MGL_TEXCOORD_OFFSET_4 + 0)
	#define T_COORD	  (MGL_TEXCOORD_OFFSET_4 + 1)
	#define C_UBYTE	  (GL_COLOR_TABLE)

	typedef float lerpvec_t[LERPSIZE];

	static lerpvec_t  *s_lerped;
	static varray_t	  vArray[MAX_VERTS];
	static vec4_t	  shadowverts[MAX_VERTS];

#endif

vec3_t	shadevector; 
float	shadelight[3]; 

static vec3_t  shadetrans; //surgeon

const float   r_avertexnormals[NUMVERTEXNORMALS][3] = { 
#include "anorms.h"
};
const float   r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "anormtab.h" 
; 

#if 1  //////////////////// 

#ifdef AMIGA_VOLATILE_ARRAYS

extern	vec3_t	lightspot; 

void GL_LerpShadowVerts( int nverts, dtrivertx_t *v, float *lerp, float move[3] )
{
	int	i;
	float	height, lheight; 
	float	*shadowdat;
 
	lheight = currententity->origin[2] - lightspot[2]; 
	height = -lheight + 1.0;

	lheight += move[2];

	shadowdat = shadowverts[0];

	for (i=0 ; i < nverts; i++, v++, lerp+=LERPSIZE, shadowdat+=4 ) 
	{ 
		shadowdat[0] = move[0] + lerp[0];
		shadowdat[1] = move[1] + lerp[1];

		shadowdat[0] -= shadevector[0]*(lerp[2]+lheight);
		shadowdat[1] -= shadevector[1]*(lerp[2]+lheight);
		shadowdat[2] = height;
	} 
}

#endif

//surgeon: move vector built into the modelview matrix

void GL_LerpVerts_notrans( int nverts, dtrivertx_t *v, dtrivertx_t *ov, dtrivertx_t *verts, float *lerp, float frontv[3], float backv[3] ) 
{ 
	int i; 

	if(ov) //lerp
	{ 
		//PMM -- added RF_SHELL_DOUBLE, RF_SHELL_HALF_DAM 
		if ( currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM) ) 
		{ 
			for (i=0 ; i < nverts; i++, v++, ov++, lerp+=LERPSIZE ) 
			{ 
				const float *normal = r_avertexnormals[verts[i].lightnormalindex]; 
 
				lerp[0] = ov->v[0]*backv[0] + v->v[0]*frontv[0] + normal[0] * POWERSUIT_SCALE; 
				lerp[1] = ov->v[1]*backv[1] + v->v[1]*frontv[1] + normal[1] * POWERSUIT_SCALE; 
				lerp[2] = ov->v[2]*backv[2] + v->v[2]*frontv[2] + normal[2] * POWERSUIT_SCALE;	
			} 
		}

		else 
		{
			if( !gl_smoothmodels->value )
			{
				for (i=0 ; i < nverts; i++, v++, ov++, lerp+=LERPSIZE)
				{
					lerp[0] = ov->v[0]*backv[0] + v->v[0]*frontv[0]; 
					lerp[1] = ov->v[1]*backv[1] + v->v[1]*frontv[1]; 
					lerp[2] = ov->v[2]*backv[2] + v->v[2]*frontv[2]; 
				}
			}

			else if( gl_vertex_arrays->value )
			{
				ULONG r,g,b,a;
				const float  *shadedots = r_avertexnormal_dots[((int)(currententity->angles[YAW] * (SHADEDOT_QUANT / 360.0)))
					& (SHADEDOT_QUANT - 1)]; 

				if (currententity->flags & RF_TRANSLUCENT)
					a = ((GLuint)(currententity->alpha * 255.f)) << 24;

				else
					a = 0xFF000000;

				shadelight[0] *= 255.f;
				shadelight[1] *= 255.f;
				shadelight[2] *= 255.f;

				for (i=0 ; i < nverts; i++, v++, ov++, lerp+=LERPSIZE)
				{
					ULONG argb = a;

					float l = shadedots[verts[i].lightnormalindex];

					r = l * shadelight[0];
					g = l * shadelight[1];
					b = l * shadelight[2];
 
					if(r > 0x000000FF) argb |= 0x00FF0000;
					else argb |= r << 16;

					if(g > 0x000000FF) argb |= 0x0000FF00;
					else argb |= g << 8;

					if(b > 0x000000FF) argb |= 0x000000FF;
					else argb |= b;

				#ifdef AMIGA_VOLATILE_ARRAYS
					((ULONG*)lerp)[C_UBYTE] = argb;
				#else 
					vArray[i].color = argb;
				#endif

					lerp[0] = ov->v[0]*backv[0] + v->v[0]*frontv[0]; 
					lerp[1] = ov->v[1]*backv[1] + v->v[1]*frontv[1]; 
					lerp[2] = ov->v[2]*backv[2] + v->v[2]*frontv[2]; 
				}
			}

			else
			{
				const float  *shadedots = r_avertexnormal_dots[((int)(currententity->angles[YAW]
					* (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];
 
				for (i=0 ; i < nverts; i++, v++, ov++, lerp+=LERPSIZE)
				{
					float l = shadedots[verts[i].lightnormalindex];

					vArray[i].data[0] = l * shadelight[0];
					vArray[i].data[1] = l * shadelight[1];
					vArray[i].data[2] = l * shadelight[2];

					lerp[0] = ov->v[0]*backv[0] + v->v[0]*frontv[0]; 
					lerp[1] = ov->v[1]*backv[1] + v->v[1]*frontv[1]; 
					lerp[2] = ov->v[2]*backv[2] + v->v[2]*frontv[2]; 
				}
			}
		} 
	}

	else
	{
		//PMM -- added RF_SHELL_DOUBLE, RF_SHELL_HALF_DAM 
		if ( currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM) ) 
		{ 
			for (i=0 ; i < nverts; i++, v++, lerp+=LERPSIZE ) 
			{ 
				const float *normal = r_avertexnormals[verts[i].lightnormalindex]; 
 
				lerp[0] = v->v[0]*frontv[0] + normal[0] * POWERSUIT_SCALE; 
				lerp[1] = v->v[1]*frontv[1] + normal[1] * POWERSUIT_SCALE; 
				lerp[2] = v->v[2]*frontv[2] + normal[2] * POWERSUIT_SCALE;  
			} 
		}

		else 
		{
			if( !gl_smoothmodels->value )
			{
				for (i=0 ; i < nverts; i++, v++, lerp+=LERPSIZE) 
				{ 
					lerp[0] = v->v[0]*frontv[0]; 
					lerp[1] = v->v[1]*frontv[1]; 
					lerp[2] = v->v[2]*frontv[2];
				}
			} 

			else if( gl_vertex_arrays->value )
			{
				ULONG r,g,b,a;
				const float  *shadedots = r_avertexnormal_dots[((int)(currententity->angles[YAW]
					* (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)]; 

				if (currententity->flags & RF_TRANSLUCENT)
					a = ((GLuint)(currententity->alpha * 255.f)) << 24;

				else
					a = 0xFF000000;
 
				shadelight[0] *= 255.f;
				shadelight[1] *= 255.f;
				shadelight[2] *= 255.f;

				for (i=0 ; i < nverts; i++, v++, lerp+=LERPSIZE) 
				{ 
					ULONG argb = a;

					float l = shadedots[verts[i].lightnormalindex];
 
					r = l * shadelight[0];
					g = l * shadelight[1];
					b = l * shadelight[2];
 
					if(r > 0x000000FF) argb |= 0x00FF0000;
					else argb |= r << 16;

					if(g > 0x000000FF) argb |= 0x0000FF00;
					else argb |= g << 8;

					if(b > 0x000000FF) argb |= 0x000000FF;
					else argb |= b;

				#ifdef AMIGA_VOLATILE_ARRAYS
					((ULONG*)lerp)[C_UBYTE] = argb;
				#else 
					vArray[i].color = argb;
				#endif

					lerp[0] = v->v[0]*frontv[0]; 
					lerp[1] = v->v[1]*frontv[1]; 
					lerp[2] = v->v[2]*frontv[2]; 
				} 
			}

			else
			{
				const float  *shadedots = r_avertexnormal_dots[((int)(currententity->angles[YAW]
					* (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)]; 

				for (i=0 ; i < nverts; i++, v++, lerp+=LERPSIZE) 
				{ 
					float l = shadedots[verts[i].lightnormalindex];

					vArray[i].data[0] = l * shadelight[0];
					vArray[i].data[1] = l * shadelight[1];
					vArray[i].data[2] = l * shadelight[2];
					lerp[0] = v->v[0]*frontv[0]; 
					lerp[1] = v->v[1]*frontv[1]; 
					lerp[2] = v->v[2]*frontv[2]; 
				} 
			}
		} 
	} 
} 

#else	///////////////////////// 

void GL_LerpVerts( int nverts, dtrivertx_t *v, dtrivertx_t *ov, dtrivertx_t *verts, float *lerp, float move[3], float frontv[3], float backv[3] ) 
{ 
	int i; 

	if(ov) //lerp
	{ 
		//PMM -- added RF_SHELL_DOUBLE, RF_SHELL_HALF_DAM 
		if ( currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM) ) 
		{ 
			for (i=0 ; i < nverts; i++, v++, ov++, lerp+=LERPSIZE ) 
			{ 
				const float *normal = r_avertexnormals[verts[i].lightnormalindex]; 
 
				lerp[0] = move[0] + ov->v[0]*backv[0] + v->v[0]*frontv[0] + normal[0] * POWERSUIT_SCALE; 
				lerp[1] = move[1] + ov->v[1]*backv[1] + v->v[1]*frontv[1] + normal[1] * POWERSUIT_SCALE; 
				lerp[2] = move[2] + ov->v[2]*backv[2] + v->v[2]*frontv[2] + normal[2] * POWERSUIT_SCALE;  
			} 
		}

		else 
		{
		
			if( !gl_smoothmodels->value )
			{
				for (i=0 ; i < nverts; i++, v++, ov++, lerp+=LERPSIZE)
				{
					lerp[0] = move[0] + ov->v[0]*backv[0] + v->v[0]*frontv[0]; 
					lerp[1] = move[1] + ov->v[1]*backv[1] + v->v[1]*frontv[1]; 
					lerp[2] = move[2] + ov->v[2]*backv[2] + v->v[2]*frontv[2]; 
				}
			}

			else if( gl_vertex_arrays->value )
			{
				ULONG r,g,b,a;
				const float  *shadedots = r_avertexnormal_dots[((int)(currententity->angles[YAW]
					* (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)]; 

				if (currententity->flags & RF_TRANSLUCENT)
					a = ((GLuint)(currententity->alpha * 255.f)) << 24;

				else
					a = 0xFF000000;

				shadelight[0] *= 255.f;
				shadelight[1] *= 255.f;
				shadelight[2] *= 255.f;

				for (i=0 ; i < nverts; i++, v++, ov++, lerp+=LERPSIZE)
				{
					ULONG argb = a;

					float l = shadedots[verts[i].lightnormalindex];

					r = l * shadelight[0];
					g = l * shadelight[1];
					b = l * shadelight[2];
 
					if(r > 0x000000FF) r = 0x00FF0000;
					else r <<= 16;

					if(g > 0x000000FF) g = 0x0000FF00;
					else g <<= 8;

					if(b > 0x000000FF) b = 0x000000FF;

				#ifdef AMIGA_VOLATILE_ARRAYS
					((ULONG*)lerp)[C_UBYTE] = a|r|g|b;
				#else 
					vArray[i].color = a|r|g|b;
				#endif

					lerp[0] = move[0] + ov->v[0]*backv[0] + v->v[0]*frontv[0]; 
					lerp[1] = move[1] + ov->v[1]*backv[1] + v->v[1]*frontv[1]; 
					lerp[2] = move[2] + ov->v[2]*backv[2] + v->v[2]*frontv[2]; 
				}
			}

			else
			{

				const float  *shadedots = r_avertexnormal_dots[((int)(currententity->angles[YAW]
					* (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];
 
				for (i=0 ; i < nverts; i++, v++, ov++, lerp+=LERPSIZE)
				{
					float l = shadedots[verts[i].lightnormalindex];

					vArray[i].data[0] = l * shadelight[0];
					vArray[i].data[1] = l * shadelight[1];
					vArray[i].data[2] = l * shadelight[2];
					lerp[0] = move[0] + ov->v[0]*backv[0] + v->v[0]*frontv[0]; 
					lerp[1] = move[1] + ov->v[1]*backv[1] + v->v[1]*frontv[1]; 
					lerp[2] = move[2] + ov->v[2]*backv[2] + v->v[2]*frontv[2]; 
				}
			}
		} 
	}

	else
	{
		//PMM -- added RF_SHELL_DOUBLE, RF_SHELL_HALF_DAM 
		if ( currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM) ) 
		{		
			for (i=0 ; i < nverts; i++, v++, lerp+=LERPSIZE ) 
			{ 
				const float *normal = r_avertexnormals[verts[i].lightnormalindex]; 
 
				lerp[0] = move[0] + v->v[0]*frontv[0] + normal[0] * POWERSUIT_SCALE; 
				lerp[1] = move[1] + v->v[1]*frontv[1] + normal[1] * POWERSUIT_SCALE; 
				lerp[2] = move[2] + v->v[2]*frontv[2] + normal[2] * POWERSUIT_SCALE;  
			} 
		}

		else 
		{		
			if( !gl_smoothmodels->value )
			{			
				for (i=0 ; i < nverts; i++, v++, lerp+=LERPSIZE) 
				{ 
					lerp[0] = move[0] + v->v[0]*frontv[0]; 
					lerp[1] = move[1] + v->v[1]*frontv[1]; 
					lerp[2] = move[2] + v->v[2]*frontv[2];
				}
			} 

			else if( gl_vertex_arrays->value )
			{
				ULONG r,g,b,a;
				const float  *shadedots = r_avertexnormal_dots[((int)(currententity->angles[YAW]
					* (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)]; 

				if (currententity->flags & RF_TRANSLUCENT)
					a = ((GLuint)(currententity->alpha * 255.f)) << 24;

				else
					a = 0xFF000000;
 
				shadelight[0] *= 255.f;
				shadelight[1] *= 255.f;
				shadelight[2] *= 255.f;

				for (i=0 ; i < nverts; i++, v++, lerp+=LERPSIZE) 
				{
					ULONG argb = a;
 
					float l = shadedots[verts[i].lightnormalindex];

					r = l * shadelight[0];
					g = l * shadelight[1];
					b = l * shadelight[2];
 
					if(r > 0x000000FF) r = 0x00FF0000;
					else r <<= 16;

					if(g > 0x000000FF) g = 0x0000FF00;
					else g <<= 8;

					if(b > 0x000000FF) b = 0x000000FF;

				#ifdef AMIGA_VOLATILE_ARRAYS
					((ULONG*)lerp)[C_UBYTE] = a|r|g|b;
				#else 
					vArray[i].color = a|r|g|b;
				#endif

					lerp[0] = move[0] + v->v[0]*frontv[0]; 
					lerp[1] = move[1] + v->v[1]*frontv[1]; 
					lerp[2] = move[2] + v->v[2]*frontv[2]; 
				} 
			}

			else
			{
				const float  *shadedots = r_avertexnormal_dots[((int)(currententity->angles[YAW]
					* (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)]; 
		
				for (i=0 ; i < nverts; i++, v++, lerp+=LERPSIZE) 
				{ 
					float l = shadedots[verts[i].lightnormalindex];

					vArray[i].data[0] = l * shadelight[0];
					vArray[i].data[1] = l * shadelight[1];
					vArray[i].data[2] = l * shadelight[2];

					lerp[0] = move[0] + v->v[0]*frontv[0]; 
					lerp[1] = move[1] + v->v[1]*frontv[1]; 
					lerp[2] = move[2] + v->v[2]*frontv[2];
				} 
			}
		} 
	} 
} 

#endif 
 
/* 
============= 
GL_DrawAliasFrameLerp 
 
interpolates between two frames and origins 
FIXME: batch lerp all vertexes 
============= 
*/ 

//Surgeon: proper vertexarray-support added
//added fast path for r_lerpmodels 0

static void GL_SetVertexArrayPointers (void)
{
	#ifdef AMIGA_VOLATILE_ARRAYS //MiniGL special feature

	s_lerped = (lerpvec_t *)mglGetVolatileVertexPointer3f();
	mglSetVolatileColorMode( 4, MGL_UBYTE_ARGB );
	glEnableClientState( MGL_VOLATILE_ARRAYS );

	#else

	glTexCoordPointer( 3, GL_FLOAT, sizeof(varray_t), (void*)vArray->data );
	glColorPointer( 4, MGL_UBYTE_ARGB, sizeof(varray_t), (void*)&vArray->color ); 
	glVertexPointer( 3, GL_FLOAT, 4*sizeof(float), (void*)s_lerped );

	#endif
}

static qboolean va_is_enabled = false;

static GLsizei vcount[1024];
static UWORD   vaindex[MAX_VERTS*4];

void GL_DrawAliasFrameLerp (dmdl_t *paliashdr, float backlerp) 
{ 
	daliasframe_t	*frame, *oldframe; 
	dtrivertx_t	*v, *ov, *verts; 
	int		*order; 
	int		count; 
	float		frontlerp; 
	float		alpha; 
	vec3_t		move, delta, vectors[3]; 
	vec3_t		frontv, backv; 
	int		i; 
	int		index_xyz; 
	GLfloat		*lerp; 
 
	if(va_is_enabled == false)
	{
	      va_is_enabled = true;
	      GL_SetVertexArrayPointers();
	}

	frame = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames + currententity->frame * paliashdr->framesize); 
	verts = v = frame->verts; 

	if(backlerp != 0.0) //surgeon
	{
		oldframe = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames + currententity->oldframe * paliashdr->framesize); 
		ov = oldframe->verts; 
	}

	else
	{
		ov = NULL;
	}

	order = (int *)((byte *)paliashdr + paliashdr->ofs_glcmds); 
 
	if (currententity->flags & RF_TRANSLUCENT) 
		alpha = currententity->alpha;
 
	else 
		alpha = 1.0; 

	// PMM - added double shell 
	if ( currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM) )
	{ 
		qglDisable( GL_TEXTURE_2D );
 
		if(gl_smoothmodels->value)
			qglShadeModel( GL_FLAT );
	}

	else if ( gl_vertex_arrays->value )
	{
		glEnableClientState( GL_TEXTURE_COORD_ARRAY );

		if(gl_smoothmodels->value)
			glEnableClientState( GL_COLOR_ARRAY );
	}

	if(backlerp != 0.0) //surgeon
	{ 
		frontlerp = 1.0 - backlerp; 
 
		// move should be the delta back to the previous frame * backlerp 

		VectorSubtract (currententity->oldorigin, currententity->origin, delta); 
		AngleVectors (currententity->angles, vectors[0], vectors[1], vectors[2]); 
 
		move[0] = DotProduct (delta, vectors[0]);	// forward 
		move[1] = -DotProduct (delta, vectors[1]);	// left 
		move[2] = DotProduct (delta, vectors[2]);	// up 
		VectorAdd (move, oldframe->translate, move); 
 
		for (i=0 ; i<3 ; i++) 
		{ 
			move[i] = backlerp*move[i] + frontlerp*frame->translate[i]; 
		} 
 
		for (i=0 ; i<3 ; i++) 
		{ 
			frontv[i] = frontlerp*frame->scale[i]; 
			backv[i] = backlerp*oldframe->scale[i]; 
		} 
	}

	else
	{
		for (i=0 ; i<3 ; i++) 
		{ 
			move[i] = frame->translate[i]; 
			frontv[i] = frame->scale[i]; 
		} 
	}

	qglTranslatef (move[0], move[1], move[2]); //surgeon: optimization

	lerp = s_lerped[0]; 

	GL_LerpVerts_notrans( paliashdr->num_xyz, v, ov, verts, lerp, frontv, backv ); 

	if(gl_shadows->value)
	{
		#ifdef AMIGA_VOLATILE_ARRAYS
		GL_LerpShadowVerts( paliashdr->num_xyz, v, lerp, move);
		#else
		VectorCopy( move, shadetrans );
		#endif
	}

	if ( currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM) )
	{
		qglColor4f(shadelight[0], shadelight[1], shadelight[2], alpha);
	}

	else if(!gl_smoothmodels->value)
	{
		float r,g,b;

		r = shadelight[0] * 1.2;
		g = shadelight[1] * 1.2;
		b = shadelight[2] * 1.2;

		qglColor4f(r, g, b, alpha);
	}

	else if( !gl_vertex_arrays->value )
	{
		qglColor4f(1, 1, 1, alpha);
	}

	if ( gl_vertex_arrays->value ) 
	{
		GLenum	curprim;
		UWORD	*idx;
  
		glEnableClientState(GL_VERTEX_ARRAY);
		glLockArrays( 0, paliashdr->num_xyz );

		while (1) 
		{ 
			// get the vertex count and primitive type
 
			count = *order++;
 
			if (!count) 
				break;		// done

			if (count < 0) 
			{ 
				count = -count; 
				curprim = GL_TRIANGLE_FAN; 
			}
 
			else 
			{ 
				curprim = GL_TRIANGLE_STRIP; 
			}

			vcount[0] = count;	// Cowcat
			idx	  = vaindex;	//

			if ( currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM) )
			{
				do 
				{
					*idx++ = (UWORD)order[2];
					order += 3; 
 
				} while (--count);
			}

			else
			{
				do 
				{
					index_xyz = order[2];

					#ifdef AMIGA_VOLATILE_ARRAYS

					((float *)(s_lerped[index_xyz]))[S_COORD] = ((float *)order)[0];
					((float *)(s_lerped[index_xyz]))[T_COORD] = ((float *)order)[1];

					#else

					vArray[index_xyz].data[0] = ((float *)order)[0];
					vArray[index_xyz].data[1] = ((float *)order)[1];

					#endif

					*idx++ = (UWORD)index_xyz;
					order += 3;
 
				} while (--count);
			}

			glDrawElements( curprim, vcount[0], GL_UNSIGNED_SHORT, (void *)vaindex);
		}

#if 0
//not in minigl aminet sources - Cowcat

		if(pcount)
		{
			mglMultiModeDrawElements( primtype, vcount, GL_UNSIGNED_SHORT, (void **)vaindices, pcount );
		}
#endif

		glUnlockArrays();
		glDisableClientState( GL_VERTEX_ARRAY );

		if ( !( currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM) ) )
		{
			glDisableClientState (GL_TEXTURE_COORD_ARRAY);

			if(gl_smoothmodels->value) // ; fix Cowcat
				glDisableClientState ( GL_COLOR_ARRAY );
		}
	}
 
	else 
	{
		while (1) 
		{ 
			// get the vertex count and primitive type 
			count = *order++; 

			if (!count) 
				break;		// done 

			if (count < 0) 
			{ 
				count = -count; 
				qglBegin (GL_TRIANGLE_FAN); 
			}
 
			else 
			{ 
				qglBegin (GL_TRIANGLE_STRIP); 
			} 
 
			if ( currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM) )
			{ 
				do 
				{ 
					index_xyz = order[2]; 
					order += 3; 

					qglVertex3fv (s_lerped[index_xyz]);

				} while (--count); 
			}

			else if(gl_smoothmodels->value)
			{
				do 
				{ 
					// texture coordinates come from the draw list
					qglTexCoord2fv((float *)order); 
					index_xyz = order[2]; 
					order += 3;

					qglColor3fv (vArray[index_xyz].data);
					qglVertex3fv (s_lerped[index_xyz]);

				} while (--count); 
			}

			else
			{
				do 
				{ 
					// texture coordinates come from the draw list 
					qglTexCoord2fv((float *)order); 
					index_xyz = order[2]; 
					order += 3;
 
					qglVertex3fv (s_lerped[index_xyz]);

				} while (--count); 
			}
 
			qglEnd (); 
		}
	} 

	if ( currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM) )
	{ 
		qglEnable( GL_TEXTURE_2D ); 

		if( gl_smoothmodels->value )
			qglShadeModel( GL_SMOOTH );
	}
}
 

#if 0 // original routine

void GL_DrawAliasFrameLerp (dmdl_t *paliashdr, float backlerp) 
{ 
	float		l; 
	daliasframe_t	*frame, *oldframe; 
	dtrivertx_t	*v, *ov, *verts; 
	int		*order; 
	int		count; 
	float		frontlerp; 
	float		alpha; 
	vec3_t		move, delta, vectors[3]; 
	vec3_t		frontv, backv; 
	int		i; 
	int		index_xyz; 
	float		*lerp; 
 
	frame = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames + currententity->frame * paliashdr->framesize); 
	verts = v = frame->verts; 
 
	oldframe = (daliasframe_t *)((byte *)paliashdr + paliashdr->ofs_frames + currententity->oldframe * paliashdr->framesize); 
	ov = oldframe->verts; 
 
	order = (int *)((byte *)paliashdr + paliashdr->ofs_glcmds); 
 
//	glTranslatef (frame->translate[0], frame->translate[1], frame->translate[2]); 
//	glScalef (frame->scale[0], frame->scale[1], frame->scale[2]); 
 
	if (currententity->flags & RF_TRANSLUCENT) 
		alpha = currententity->alpha; 
	else 
		alpha = 1.0; 

	// PMM - added double shell 

	if ( currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM) ) 
		qglDisable( GL_TEXTURE_2D ); 
 
	frontlerp = 1.0 - backlerp; 
 
	// move should be the delta back to the previous frame * backlerp 
	VectorSubtract (currententity->oldorigin, currententity->origin, delta); 
	AngleVectors (currententity->angles, vectors[0], vectors[1], vectors[2]); 
 
	move[0] = DotProduct (delta, vectors[0]);	// forward 
	move[1] = -DotProduct (delta, vectors[1]);	// left 
	move[2] = DotProduct (delta, vectors[2]);	// up 
 
	VectorAdd (move, oldframe->translate, move); 
 
	for (i=0 ; i<3 ; i++) 
	{ 
		move[i] = backlerp*move[i] + frontlerp*frame->translate[i]; 
	} 
 
	for (i=0 ; i<3 ; i++) 
	{ 
		frontv[i] = frontlerp*frame->scale[i]; 
		backv[i] = backlerp*oldframe->scale[i]; 
	} 
 
	lerp = s_lerped[0]; 
 
	GL_LerpVerts( paliashdr->num_xyz, v, ov, verts, lerp, move, frontv, backv ); 
 
	if ( gl_vertex_arrays->value ) 
	{ 
		static float colorArray[MAX_VERTS*4]; 
 
		qglEnableClientState( GL_VERTEX_ARRAY );
		qglVertexPointer( 3, GL_FLOAT, 16, s_lerped ); // padded for SIMD

		// PMM - added double damage shell 
		if ( currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM) ) 
		{ 
			qglColor4f( shadelight[0], shadelight[1], shadelight[2], alpha ); 
		} 
		else 
		{ 
			qglEnableClientState( GL_COLOR_ARRAY ); 
			qglColorPointer( 3, GL_FLOAT, 0, colorArray ); 
 
			// 
			// pre light everything 
			// 
			for ( i = 0; i < paliashdr->num_xyz; i++ ) 
			{ 
				float l = shadedots[verts[i].lightnormalindex]; 
 
				colorArray[i*3+0] = l * shadelight[0]; 
				colorArray[i*3+1] = l * shadelight[1]; 
				colorArray[i*3+2] = l * shadelight[2]; 
			} 
		} 
 
		if ( qglLockArraysEXT != 0 ) 
			qglLockArraysEXT( 0, paliashdr->num_xyz );

		while (1) 
		{ 
			// get the vertex count and primitive type 
			count = *order++;

			if (!count) 
				break;		// done

			if (count < 0) 
			{ 
				count = -count; 
				qglBegin (GL_TRIANGLE_FAN); 
			}

			else 
			{ 
				qglBegin (GL_TRIANGLE_STRIP); 
			} 
 
			// PMM - added double damage shell 
			if ( currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM) ) 
			{
				do 
				{ 
					index_xyz = order[2]; 
					order += 3; 
 
					qglVertex3fv( s_lerped[index_xyz] ); 
				} while (--count);
			}

			else 
			{ 
				do 
				{ 
					// texture coordinates come from the draw list
					index_xyz = order[2]; 

					qglTexCoord2f (((float *)order)[0], ((float *)order)[1]);
					order += 3; 
 
					glArrayElement( index_xyz ); 
 
				} while (--count); 
			}

			qglEnd (); 
		} 

		if ( qglUnlockArraysEXT != 0 ) 
			qglUnlockArraysEXT(); 
	}

	else 
	{ 
		while (1) 
		{ 
			// get the vertex count and primitive type 
			count = *order++;

			if (!count) 
				break;		// done

			if (count < 0) 
			{ 
				count = -count; 
				qglBegin (GL_TRIANGLE_FAN); 
			}

			else 
			{ 
				qglBegin (GL_TRIANGLE_STRIP); 
			} 
 
			if ( currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE ) ) 
			{ 
				do 
				{ 
					index_xyz = order[2]; 
					order += 3; 
 
					qglColor4f( shadelight[0], shadelight[1], shadelight[2], alpha); 
					qglVertex3fv (s_lerped[index_xyz]); 
 
				} while (--count); 
			}

			else 
			{ 
				do 
				{ 
					// texture coordinates come from the draw list 
					qglTexCoord2f (((float *)order)[0], ((float *)order)[1]); 
					index_xyz = order[2]; 
					order += 3; 
 
					// normals and vertexes come from the frame list 
					l = shadedots[verts[index_xyz].lightnormalindex]; 
					 
					qglColor4f (l* shadelight[0], l*shadelight[1], l*shadelight[2], alpha); 
					qglVertex3fv (s_lerped[index_xyz]);
 
				} while (--count); 
			} 
 
			qglEnd (); 
		} 
	} 
 
	// if ( currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE ) )
 
	// PMM - added double damage shell 
	if ( currententity->flags & ( RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM) ) 
		qglEnable( GL_TEXTURE_2D ); 
}

#endif 

extern	vec3_t	lightspot; 
	 
/* 
============= 
GL_DrawAliasShadow 
============= 
*/

void GL_DrawAliasShadow (entity_t *e, dmdl_t *paliashdr) // , int posenum) 
{ 
	int	*order; 
	vec3_t	point; 
	float	height, lheight, an; 
	int	count; 
	
	lheight = currententity->origin[2] - lightspot[2]; 
	height = -lheight + 0.1; // was 1.0

	order = (int *)((byte *)paliashdr + paliashdr->ofs_glcmds); 
 
	// Knightmare- don't draw shadows above view origin
	if (r_newrefdef.vieworg[2] < (currententity->origin[2] + height))
		return;

	// faster alternative ? - Cowcat
	float shadescale = 0.707106781187;
	an = currententity->angles[1] / 180 * M_PI;

	shadevector[0] = cos(-an) * shadescale;
	shadevector[1] = sin(-an) * shadescale;
	shadevector[2] = shadescale;

	qglPushMatrix ();

	qglTranslatef(e->origin[0], e->origin[1], e->origin[2]);	
	glRotatefEXT(e->angles[1],GLROT_001); 
 
	qglDisable (GL_TEXTURE_2D);
	qglEnable (GL_BLEND);
 
	qglColor4f (0,0,0,0.5);

#ifdef AMIGAOS //

	#ifndef AMIGA_VOLATILE_ARRAYS
	lheight += shadetrans[2];
	#endif

	#if 0 // doesn´t work as supposed (missing particles & effects) - Cowcat

	if(gl_vertex_arrays->value) //surgeon
	{
		int 	i;
		GLenum 	curprim;
		UWORD 	*idx;

		#ifndef AMIGA_VOLATILE_ARRAYS

		for(i=0; i<paliashdr->num_xyz; i++)
		{
			float *v = s_lerped[i];

			v[0] += shadetrans[0];
			v[1] += shadetrans[1];

			v[0] -= shadevector[0]*(v[2]+lheight); 
			v[1] -= shadevector[1]*(v[2]+lheight); 

			v[2] = height; 
		}

		#endif

		#ifdef AMIGA_VOLATILE_ARRAYS

		glDisableClientState(MGL_VOLATILE_ARRAYS);
		glVertexPointer(3, GL_FLOAT, 4*sizeof(float), (void *)shadowverts);

		#endif

		glEnableClientState(GL_VERTEX_ARRAY);
		glLockArrays(0, paliashdr->num_xyz); 
	  
		while (1) 
		{ 
			// get the vertex count and primitive type 
			count = *order++;

			if (!count) 
				break;		// done
 
			if (count < 0) 
			{ 
				count = -count; 
				curprim = GL_TRIANGLE_FAN; 
			}
 
			else
			{ 
				curprim = GL_TRIANGLE_STRIP; 
			}

			idx = vaindex;
			vcount[0] = count;

			do 
			{
				*idx++ = (UWORD)order[2];
				order += 3; 
 
			} while (--count);

			glDrawElements( curprim, vcount[0], GL_UNSIGNED_SHORT, (void *)vaindex); 
		}

		//mglMultiModeDrawElements( primtype, vcount, GL_UNSIGNED_SHORT, (void **)vaindices, pcount );
	
		glUnlockArrays();
		glDisableClientState(GL_VERTEX_ARRAY);

		#ifdef AMIGA_VOLATILE_ARRAYS
		glEnableClientState(MGL_VOLATILE_ARRAYS);
		#endif
	}

	else
	#endif // no vertexarrays

	{
#endif
		while (1) 
		{ 
			// get the vertex count and primitive type 
			count = *order++;
 
			if (!count) 
				break;		// done
 
		if (count < 0) 
		{ 
			count = -count; 
			qglBegin (GL_TRIANGLE_FAN); 
		}

		else 
			qglBegin (GL_TRIANGLE_STRIP); 
 
		do 
		{ 
			// normals and vertexes come from the frame list 
			memcpy( point, s_lerped[order[2]], sizeof( point )  );
 
			#ifdef AMIGAOS

			#ifndef AMIGA_VOLATILE_ARRAYS

			point[0] += shadetrans[0];
			point[1] += shadetrans[1];

			point[0] -= shadevector[0]*(point[2]+lheight); 
			point[1] -= shadevector[1]*(point[2]+lheight); 
			point[2] = height; 

			qglVertex3fv (point);
 
			#else

			qglVertex3fv (shadowverts[order[2]]);
 
			#endif

			#else

			point[0] -= shadevector[0]*(point[2]+lheight); 
			point[1] -= shadevector[1]*(point[2]+lheight); 
			point[2] = height; 

			qglVertex3fv (point);
 
			#endif
 
			order += 3; 
 
		} while (--count); 
 
		qglEnd (); 
	}
#ifdef AMIGAOS
	}	
		
#endif

	qglEnable (GL_TEXTURE_2D);
	qglDisable (GL_BLEND);
	qglPopMatrix ();
	
}


#ifdef AMIGAOS
//static ULONG frustum_intersect = 1; //surgeon: for gl_ext_clip_volume_hint. 
#endif

/* 
** R_CullAliasModel 
*/

static qboolean R_CullAliasModel( vec3_t bbox[8], entity_t *e ) 
{ 
	int 		i; 
	vec3_t		mins, maxs; 
	dmdl_t		*paliashdr; 
	vec3_t		vectors[3]; 
	vec3_t		thismins, oldmins, thismaxs, oldmaxs; 
	daliasframe_t	*pframe, *poldframe; 
	vec3_t		angles; 
 
	#ifdef AMIGAOS

	//extern cvar_t *gl_ext_clip_volume_hint; //surgeon - not used - Cowcat
	paliashdr = *(dmdl_t **)currentmodel->extradata;
 
	#else

	paliashdr = (dmdl_t *)currentmodel->extradata;
 
	#endif 
 
	if ( ( e->frame >= paliashdr->num_frames ) || ( e->frame < 0 ) ) 
	{ 
		ri.Con_Printf (PRINT_ALL, "R_CullAliasModel %s: no such frame %d\n", currentmodel->name, e->frame); 
		e->frame = 0; 
	}

	if ( ( e->oldframe >= paliashdr->num_frames ) || ( e->oldframe < 0 ) ) 
	{ 
		ri.Con_Printf (PRINT_ALL, "R_CullAliasModel %s: no such oldframe %d\n", currentmodel->name, e->oldframe); 
		e->oldframe = 0; 
	} 
 
	pframe = ( daliasframe_t * ) ( ( byte * ) paliashdr + paliashdr->ofs_frames + e->frame * paliashdr->framesize); 
	poldframe = ( daliasframe_t * ) ( ( byte * ) paliashdr + paliashdr->ofs_frames + e->oldframe * paliashdr->framesize); 
 
	/* 
	** compute axially aligned mins and maxs 
	*/

	if ( pframe == poldframe ) 
	{ 
		for ( i = 0; i < 3; i++ ) 
		{ 
			mins[i] = pframe->translate[i]; 
			maxs[i] = mins[i] + pframe->scale[i]*255; 
		} 
	}
 
	else 
	{ 
		for ( i = 0; i < 3; i++ ) 
		{ 
			thismins[i] = pframe->translate[i]; 
			thismaxs[i] = thismins[i] + pframe->scale[i]*255; 
 
			oldmins[i]  = poldframe->translate[i]; 
			oldmaxs[i]  = oldmins[i] + poldframe->scale[i]*255; 
 
			if ( thismins[i] < oldmins[i] ) 
				mins[i] = thismins[i];

			else 
				mins[i] = oldmins[i]; 
 
			if ( thismaxs[i] > oldmaxs[i] ) 
				maxs[i] = thismaxs[i];
 
			else 
				maxs[i] = oldmaxs[i]; 
		} 
	} 
 
	/* 
	** compute a full bounding box 
	*/
 
	for ( i = 0; i < 8; i++ ) 
	{ 
		vec3_t	 tmp; 
 
		if ( i & 1 ) 
			tmp[0] = mins[0];
 
		else 
			tmp[0] = maxs[0]; 
 
		if ( i & 2 ) 
			tmp[1] = mins[1];

		else 
			tmp[1] = maxs[1]; 
 
		if ( i & 4 ) 
			tmp[2] = mins[2];
 
		else 
			tmp[2] = maxs[2]; 
 
		VectorCopy( tmp, bbox[i] ); 
	} 
 
	/* 
	** rotate the bounding box 
	*/
 
	VectorCopy( e->angles, angles ); 
	angles[YAW] = -angles[YAW]; 
	AngleVectors( angles, vectors[0], vectors[1], vectors[2] ); 
 
	for ( i = 0; i < 8; i++ ) 
	{ 
		vec3_t tmp; 
 
		VectorCopy( bbox[i], tmp ); 
 
		bbox[i][0] = DotProduct( vectors[0], tmp ); 
		bbox[i][1] = -DotProduct( vectors[1], tmp ); 
		bbox[i][2] = DotProduct( vectors[2], tmp ); 
 
		VectorAdd( e->origin, bbox[i], bbox[i] ); 
	} 

	#ifdef AMIGAOS
	//if(gl_ext_clip_volume_hint->value) 
		//frustum_intersect = 0;
	#endif
 
	{ 
		int p, f, aggregatemask = ~0; 

		for ( p = 0; p < 8; p++ ) 
		{ 
			int mask = 0; 
 
			for ( f = 0; f < 4; f++ ) 
			{ 
				float dp = DotProduct( frustum[f].normal, bbox[p] ); 
 
				if ( ( dp - frustum[f].dist ) < 0 ) 
				{ 
					mask |= ( 1 << f ); 
				} 
			}
 
			//frustum_intersect |= mask; //surgeon

			aggregatemask &= mask; 
		} 
 
		if ( aggregatemask ) 
		{ 
			return true; 
		} 
 
		return false; 
	} 
} 
 
/* 
================= 
R_DrawAliasModel 
 
================= 
*/ 

extern cvar_t *gl_showbbox;

void R_DrawAliasModelBBox (vec3_t bbox[8], entity_t *e) 
{	
	if (currententity->flags & (RF_WEAPONMODEL | RF_VIEWERMODEL | RF_BEAM ))
		return;

	int vector[24] = { 
		0,1,3,2, // up quad
		2,0,3,1, 
		4,5,7,6, // down quad
		6,4,7,5, 
		4,0,5,1, // pair corners back or front
		6,2,7,3  // pair corners front or back
	};

	int mode, i;

	mode = (int)gl_showbbox->value;

	if(mode == 2)
		glDisable(GL_DEPTH_TEST);

	//qglColor4f(0.5f,0.5f,0.5f,0.2f);
	qglColor4f(1,1,1,1);

	glEnable( GL_BLEND );
        qglDisable( GL_CULL_FACE ); 
        qglDisable( GL_TEXTURE_2D );

	// Workaround for missing glPolygonMode GL_LINE - Cowcat

	qglBegin ( GL_LINES );

	for(i=0; i < 24; i++)
	{
		qglVertex3fv( bbox[vector[i]] );
	}

	qglEnd();
		
	if(mode == 2)
		glEnable(GL_DEPTH_TEST);

	glDisable( GL_BLEND );	
        qglEnable( GL_TEXTURE_2D );  
        qglEnable( GL_CULL_FACE );
}

void R_FlipModel (qboolean on, qboolean cullOnly) //Knightmare
{
	extern void MYgluPerspective( GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar ); 

	if (on) 
	{
		if(!cullOnly)
		{
			qglMatrixMode( GL_PROJECTION ); 
			qglPushMatrix(); 
			qglLoadIdentity(); 
			qglScalef( -1, 1, 1 ); 
			MYgluPerspective( r_newrefdef.fov_y, (float)r_newrefdef.width / r_newrefdef.height, 4,  4096); 
			qglMatrixMode( GL_MODELVIEW );
		} 
 
		qglCullFace( GL_BACK ); 
	} 

	else
	{
		if (!cullOnly) 
		{ 
			qglMatrixMode( GL_PROJECTION ); 
			qglPopMatrix(); 
			qglMatrixMode( GL_MODELVIEW );
		}

		qglCullFace( GL_FRONT ); 
	}
}

void R_DrawAliasModel (entity_t *e) 
{ 
	int		i; 
	dmdl_t		*paliashdr; 
	vec3_t		bbox[8]; 
	image_t		*skin;
	qboolean	mirrormodel = false; // Knightmare

	#ifdef AMIGAOS
	//frustum_intersect = 1; //surgeon
	#endif 

	if ( !( e->flags & RF_WEAPONMODEL || e->flags & RF_VIEWERMODEL ) ) 
	{ 
		if ( R_CullAliasModel( bbox, e ) ) 
			return; 
	} 
 
	if ( e->flags & RF_WEAPONMODEL ) 
	{ 
		if ( r_lefthand->value == 2 ) 
			return;

		else if (r_lefthand->value == 1 )
			mirrormodel = true;
	}

	else if ( e->flags & RF_MIRRORMODEL)
		mirrormodel = true;
 
	#ifdef AMIGAOS
	paliashdr = *(dmdl_t **)currentmodel->extradata;
	#else
	paliashdr = (dmdl_t *)currentmodel->extradata;
	#endif 
 
	// 
	// get lighting information 
	// 
	// PMM - rewrote, reordered to handle new shells & mixing 
	// PMM - 3.20 code .. replaced with original way of doing it to keep mod authors happy 
	// 
	if ( currententity->flags & ( RF_SHELL_HALF_DAM | RF_SHELL_GREEN | RF_SHELL_RED | RF_SHELL_BLUE | RF_SHELL_DOUBLE ) ) 
	{ 
		VectorClear (shadelight);
 
		if (currententity->flags & RF_SHELL_HALF_DAM) 
		{ 
				shadelight[0] = 0.56; 
				shadelight[1] = 0.59; 
				shadelight[2] = 0.45; 
		}
 
		if ( currententity->flags & RF_SHELL_DOUBLE ) 
		{ 
			shadelight[0] = 0.9; 
			shadelight[1] = 0.7; 
		}
 
		if ( currententity->flags & RF_SHELL_RED ) 
			shadelight[0] = 1.0;
 
		if ( currententity->flags & RF_SHELL_GREEN ) 
			shadelight[1] = 1.0;
 
		if ( currententity->flags & RF_SHELL_BLUE ) 
			shadelight[2] = 1.0; 
	} 

	else if ( currententity->flags & RF_FULLBRIGHT ) 
	{ 
		for (i=0 ; i<3 ; i++) 
			shadelight[i] = 1.0; 
	}

	else 
	{ 
		R_LightPoint (currententity->origin, shadelight); 
 
		// player lighting hack for communication back to server 
		// big hack! 
		if ( currententity->flags & RF_WEAPONMODEL ) 
		{ 
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
		 
		if ( gl_monolightmap->string[0] != '0' ) 
		{ 
			float s = shadelight[0]; 
 
			if ( s < shadelight[1] ) 
				s = shadelight[1];
 
			if ( s < shadelight[2] ) 
				s = shadelight[2]; 
 
			shadelight[0] = s; 
			shadelight[1] = s; 
			shadelight[2] = s; 
		} 
	} 
 
	if ( currententity->flags & RF_MINLIGHT ) 
	{ 
		for (i=0 ; i<3 ; i++) 
			if (shadelight[i] > 0.1) 
				break;
 
		if (i == 3) 
		{ 
			shadelight[0] = 0.1; 
			shadelight[1] = 0.1; 
			shadelight[2] = 0.1; 
		} 
	} 
 
	if ( currententity->flags & RF_GLOW ) 
	{
	 	// bonus items will pulse with time 
		float	scale; 
		float	min; 
 
		scale = 0.1 * sin(r_newrefdef.time*7);
 
		for (i=0 ; i<3 ; i++) 
		{ 
			min = shadelight[i] * 0.8; 
			shadelight[i] += scale;
 
			if (shadelight[i] < min) 
				shadelight[i] = min; 
		} 
	} 

// ================= 
// PGM	ir goggles color override 
	if ( r_newrefdef.rdflags & RDF_IRGOGGLES && currententity->flags & RF_IR_VISIBLE ) 
	{ 
		shadelight[0] = 1.0; 
		shadelight[1] = 0.0; 
		shadelight[2] = 0.0; 
	} 
// PGM	 
// ================= 

#if 0 //surgeon: moved to GL_LerpVerts

	shadedots = r_avertexnormal_dots[((int)(currententity->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];

#endif

	#if 0 // now on drawaliasshadows - Cowcat
	//surgeon: only do this is shadows are enabled:
	if(gl_shadows->value)
	{	  
		an = currententity->angles[1]/180*M_PI; 
		shadevector[0] = cos(-an); 
		shadevector[1] = sin(-an); 
		shadevector[2] = 1; 
		VectorNormalize (shadevector);
	}
	#endif

	// 
	// locate the proper data 
	// 
 
	c_alias_polys += paliashdr->num_tris; 
 
	// 
	// draw all the triangles 
	// 
	if (currententity->flags & RF_DEPTHHACK) // hack the depth range to prevent view model from poking into walls 
		qglDepthRange (gldepthmin, gldepthmin + 0.3*(gldepthmax-gldepthmin)); 
 
	if (mirrormodel)
		R_FlipModel (true, false); 
 
	qglPushMatrix (); 

	e->angles[PITCH] = -e->angles[PITCH];	// sigh. 
	R_RotateForEntity (e); 
	e->angles[PITCH] = -e->angles[PITCH];	// sigh. 
 
	// select skin 
	if (currententity->skin) 
		skin = currententity->skin;	// custom player skin
 
	else 
	{ 
		if (currententity->skinnum >= MAX_MD2SKINS) 
			skin = currentmodel->skins[0];
 
		else 
		{ 
			skin = currentmodel->skins[currententity->skinnum];

			if (!skin) 
				skin = currentmodel->skins[0]; 
		} 
	}
 
	if (!skin) 
		skin = r_notexture;	// fallback... 

	GL_Bind(skin->texnum); 
 
	// draw it 
 
	#ifndef AMIGAOS //surgeon: state sorted - only reset if shadows are rendered

	qglShadeModel ( GL_SMOOTH );

	GL_TexEnv( GL_MODULATE );

	if ( currententity->flags & RF_TRANSLUCENT ) 
	{ 
		qglEnable ( GL_BLEND ); 
	}

	#endif 

	if ( (currententity->frame >= paliashdr->num_frames) || (currententity->frame < 0) ) 
	{ 
		ri.Con_Printf (PRINT_ALL, "R_DrawAliasModel %s: no such frame %d\n", currentmodel->name, currententity->frame); 
		currententity->frame = 0; 
		currententity->oldframe = 0; 
	} 
 
	if ( (currententity->oldframe >= paliashdr->num_frames) || (currententity->oldframe < 0)) 
	{ 
		ri.Con_Printf (PRINT_ALL, "R_DrawAliasModel %s: no such oldframe %d\n", currentmodel->name, currententity->oldframe); 
		currententity->frame = 0; 
		currententity->oldframe = 0; 
	} 

	#ifdef AMIGAOS

	//if(frustum_intersect == 0) // turn off clip-test
	{ 
		//glHint(CLIP_VOLUME_CLIPPING_HINT_EXT, GL_FASTEST); // NOT AVAILABLE Cowcat
	}

	#endif

	if ( !r_lerpmodels->value ) 
		currententity->backlerp = 0; 

	GL_DrawAliasFrameLerp (paliashdr, currententity->backlerp); 

	#ifdef AMIGAOS

	//if(frustum_intersect == 0) // clip-test back on
	{
		//glHint(CLIP_VOLUME_CLIPPING_HINT_EXT, GL_NICEST); // NOT AVAILABLE Cowcat
		//frustum_intersect = 1;
	}

	#endif
 
	#ifndef AMIGAOS
	GL_TexEnv( GL_REPLACE ); 
	qglShadeModel ( GL_FLAT ); 
	#endif
 
	qglPopMatrix (); 

	if(gl_showbbox->value)
		R_DrawAliasModelBBox (bbox, e);
 
	if (mirrormodel)
		R_FlipModel (false, false);
 
	#ifndef AMIGAOS //moved because of state-sorting
	if ( currententity->flags & RF_TRANSLUCENT ) 
	{ 
		qglDisable ( GL_BLEND ); 
	} 
	#endif

	if (currententity->flags & RF_DEPTHHACK) 
		qglDepthRange (gldepthmin, gldepthmax); 

	if ( gl_shadows->value && !(currententity->flags & (RF_TRANSLUCENT | RF_WEAPONMODEL | RF_NOSHADOW)) ) 
	{
		#ifdef AMIGAOS

		//GL_TexEnv( GL_REPLACE ); 

		if(gl_smoothmodels->value)
			qglShadeModel ( GL_FLAT );
		#endif

		GL_DrawAliasShadow (e, paliashdr); // , currententity->frame );
 
		#ifdef AMIGAOS //reset texenv and shademodel to default

		if(gl_smoothmodels->value)
			qglShadeModel ( GL_SMOOTH );

		//GL_TexEnv( GL_MODULATE );

		#endif 
	} 

	qglColor4f (1,1,1,1); 
}
