//surgeon: added AMIGA_ARGB_4444 define for MGL_UNSIGNED_SHORT_4_4_4_4 lightmapformat

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
// GL_RSURF.C: surface-related refresh code 
#include <assert.h> 
#include <ctype.h> // Cowcat

#include "gl_local.h" 

static vec3_t   modelorg;	// relative to viewpoint 
 
msurface_t      *r_alpha_surfaces; 
 
#define DYNAMIC_LIGHT_WIDTH  128 
#define DYNAMIC_LIGHT_HEIGHT 128 

#ifdef AMIGA_ARGB_4444 //surgeon
	#define LIGHTMAP_BYTES 2 
#else
	#define LIGHTMAP_BYTES 4 
#endif
 
#define BLOCK_WIDTH     128 
#define BLOCK_HEIGHT    128 

#define MAX_LIGHTMAPS   128 

int	c_visible_lightmaps; 
int	c_visible_textures; 

qboolean arb_multitexture = false; //surgeon

#ifdef AMIGAOS
qboolean mgl_arb_multitexture = false; //surgeon
qboolean mgl_arb_enabled = false;
#endif 

//surgeon
#ifdef AMIGA_ARGB_4444
#define GL_LIGHTMAP_FORMAT MGL_UNSIGNED_SHORT_4_4_4_4 
#else
#define GL_LIGHTMAP_FORMAT GL_RGBA 
#endif

#ifdef AMIGAOS
extern int gl_lmaps_drawn; 
extern int gl_bpolies_drawn; 
#endif 
 
typedef struct 
{ 
	int 		internal_format; 
	int     	current_lightmap_texture; 
 
	msurface_t      *lightmap_surfaces[MAX_LIGHTMAPS]; 
 
	int             allocated[BLOCK_WIDTH]; 
 
	// the lightmap texture data needs to be kept in 
	// main memory so texsubimage can update properly 
	byte            lightmap_buffer[4*BLOCK_WIDTH*BLOCK_HEIGHT];
 
} gllightmapstate_t; 
 
static gllightmapstate_t gl_lms; 

#ifdef AMIGAOS
//surgeon:
//keep track of lightmapsurfs for vertexlight generation

static msurface_t *vlightsurfs[4096];
static int 	numvlightsurfs = 0;

#endif
 
static void	LM_InitBlock( void ); 
static void	LM_UploadBlock( qboolean dynamic ); 
static qboolean LM_AllocBlock (int w, int h, int *x, int *y); 
 
extern void 	R_SetCacheState( msurface_t *surf ); 
extern void 	R_BuildLightMap (msurface_t *surf, byte *dest, int stride); 
 
/* 
============================================================= 
 
	BRUSH MODELS 
 
============================================================= 
*/ 
 
/* 
=============== 
R_TextureAnimation 
 
Returns the proper texture for a given time and base texture 
=============== 
*/ 

image_t *R_TextureAnimation (mtexinfo_t *tex) 
{ 
	int	c; 
 
	if (!tex->next) 
		return tex->image; 
 
	c = currententity->frame % tex->numframes;
 
	while (c) 
	{ 
		tex = tex->next; 
		c--; 
	} 
 
	return tex->image; 
} 

#ifdef AMIGAOS
//vertexlighting by surgeon

extern cvar_t *r_vertexlighting;

static inline GLubyte *ReadVertexColor (float *coord, GLubyte *base)
{
	static int 	u, v, offset;
	static UWORD 	*out;
	static GLubyte 	color[4];

	u = (BLOCK_WIDTH  * coord[0]);
	v = (BLOCK_HEIGHT * coord[1]);

	offset = (u + v * BLOCK_WIDTH) * LIGHTMAP_BYTES;

	out = (UWORD *)(base + offset);

	#ifdef AMIGA_ARGB_4444

	color[0] = (*out & 0x0f00) >> 4;
	color[1] = (*out & 0x00f0)     ;
	color[2] = (*out & 0x000f) << 4;
	color[3] = 0xff;

	#else

	color[0] = (*out & 0xff00) >> 8;
	color[1] = (*out & 0x00ff)     ;
	out++;
	color[2] = (*out & 0xff00) >> 8;
	color[3] = 0xff;

	#endif

	return color;
}

void DrawGLPoly_VLight (msurface_t *fa)
{ 
	int             i; 
	glpoly_t 	*p;
	float   	*v;

	p = fa->polys; 
	
	qglBegin (GL_POLYGON);
 
	v = p->verts[0];

	for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE) 
	{ 
		qglColor3ubv((GLubyte *)&(v[7]));
		qglTexCoord2fv (&v[3]);
		qglVertex3fv (v); 
	}

	qglEnd (); 

	gl_bpolies_drawn++; 
} 
 
void DrawGLFlowingPoly_VLight (msurface_t *fa)
{ 
	int             i; 
	float   	*v; 
	glpoly_t 	*p; 
	float   	scroll; 

	scroll = -64 * ( (r_newrefdef.time / 40.0) - (int)(r_newrefdef.time / 40.0) );

	if(scroll == 0.0) 
		scroll = -64.0; 
 
	p = fa->polys; 

	qglBegin (GL_POLYGON); 

	v = p->verts[0];
 
	for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE) 
	{ 
		qglColor3ubv((GLubyte *)&(v[7]));
		qglTexCoord2f ((v[3] + scroll), v[4]);
 		qglVertex3fv (v); 
	} 

	qglEnd (); 

	gl_bpolies_drawn++;
} 

#endif

/* 
================ 
DrawGLPoly 
================ 
*/

void DrawGLPoly (glpoly_t *p )
{ 
	int      i; 
	float    *v;

	#ifdef AMIGAOS

   	if( mgl_arb_enabled )
   	{	
		qglBegin (GL_POLYGON); 
		v = p->verts[0];
 
		for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE) 
		{
			qglMultiTexCoord2fARB (GL_TEXTURE0_ARB, v[3], v[4]);
			qglMultiTexCoord2fARB (GL_TEXTURE1_ARB, v[5], v[6]);
			qglVertex3fv (v); 
		}

		qglEnd ();
   	}

   	else
   	{
		qglBegin (GL_POLYGON); 
		v = p->verts[0];
 
		for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE) 
		{ 
			qglTexCoord2f (v[3], v[4]); 
			qglVertex3fv (v); 
		}

		qglEnd (); 
   	}

	#else 

	qglBegin (GL_POLYGON); 
	v = p->verts[0];

	for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE) 
	{ 
		qglTexCoord2f (v[3], v[4]); 
		qglVertex3fv (v); 
	}

	qglEnd ();
 
	#endif
	 
	#ifdef AMIGAOS
	gl_bpolies_drawn++; 
	#endif 
 
} 
 
//============ 
//PGM 
/* 
================ 
DrawGLFlowingPoly -- version of DrawGLPoly that handles scrolling texture 
================ 
*/

void DrawGLFlowingPoly (msurface_t *fa)
{ 
	int             i; 
	float   	*v; 
	glpoly_t 	*p; 
	float   	scroll; 
 
	p = fa->polys; 
 
	scroll = -64 * ( (r_newrefdef.time / 40.0) - (int)(r_newrefdef.time / 40.0) );

	if(scroll == 0.0) 
		scroll = -64.0; 
 
	#ifdef AMIGAOS

   	if( mgl_arb_enabled )
   	{
		qglBegin (GL_POLYGON); 
		v = p->verts[0];
 
		for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE) 
		{
			qglMultiTexCoord2fARB (GL_TEXTURE0_ARB, (v[3]+scroll), v[4]); // missing scroll ? - Cowcat
			qglMultiTexCoord2fARB (GL_TEXTURE1_ARB, v[5], v[6]);
			qglVertex3fv (v); 
		}
 
		qglEnd ();
   	}

   	else
   	{
		qglBegin (GL_POLYGON); 
		v = p->verts[0];
 
		for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE) 
		{ 
			qglTexCoord2f ((v[3] + scroll), v[4]); 
			qglVertex3fv (v); 
		}

		qglEnd (); 
   	}

	#else

	qglBegin (GL_POLYGON); 
	v = p->verts[0];
 
	for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE) 
	{ 
		qglTexCoord2f ((v[3] + scroll), v[4]); 
		qglVertex3fv (v); 
	}
 
	qglEnd ();

	#endif

	#ifdef AMIGAOS
	gl_bpolies_drawn++; //added by surgeon
	#endif 

}

//PGM 
//============ 
 
/* 
** R_DrawTriangleOutlines 
*/

void R_DrawTriangleOutlines (void) 
{ 
	int		i, j, mode; 
	glpoly_t	*p; 
 
	mode = (int)gl_showtris->value; 
 
	if (mode == 2)
		qglDisable (GL_DEPTH_TEST); 

	qglDisable (GL_TEXTURE_2D);
	qglColor4f (1,1,1,1); 
 
	for (i=0 ; i<MAX_LIGHTMAPS ; i++) 
	{ 
		msurface_t *surf; 
 
		for ( surf = gl_lms.lightmap_surfaces[i]; surf != 0; surf = surf->lightmapchain ) 
		{ 
			p = surf->polys;

			for ( ; p ; p=p->chain) 
			{ 
				for (j=2 ; j<p->numverts ; j++ ) 
				{ 
					#ifdef AMIGAOS

					qglBegin (GL_LINES);
					qglVertex3fv (p->verts[0]); 
					qglVertex3fv (p->verts[j-1]);
					qglVertex3fv (p->verts[j-1]);
					qglVertex3fv (p->verts[j]); 
					qglVertex3fv (p->verts[j]); 
					qglVertex3fv (p->verts[0]);
 
					#else

					qglBegin (GL_LINE_STRIP); 
					qglVertex3fv (p->verts[0]); 
					qglVertex3fv (p->verts[j-1]); 
					qglVertex3fv (p->verts[j]); 
					qglVertex3fv (p->verts[0]);

					#endif

					qglEnd (); 
				} 
			} 
		} 
	} 

	if (mode == 2)
		qglEnable (GL_DEPTH_TEST);

	qglEnable (GL_TEXTURE_2D); 
} 
 
/* 
** DrawGLPolyChain 
*/

void DrawGLPolyChain( glpoly_t *p, float soffset, float toffset ) 
{
	if ( soffset == 0 && toffset == 0 ) 
	{ 
		for ( ; p != 0; p = p->chain ) 
		{ 
			float *v; 
			int j; 
 
			qglBegin (GL_POLYGON); 
			v = p->verts[0];
 
			for (j=0 ; j<p->numverts ; j++, v+= VERTEXSIZE) 
			{ 
				qglTexCoord2f (v[5], v[6] ); 
				qglVertex3fv (v); 
			}

			qglEnd ();
 
			#ifdef AMIGAOS
			gl_lmaps_drawn++; 
			#endif 
		} 
	}

	else
	{ 
		for ( ; p != 0; p = p->chain ) 
		{ 
			float *v; 
			int j; 
 
			qglBegin (GL_POLYGON); 
			v = p->verts[0];
 
			for (j=0 ; j<p->numverts ; j++, v+= VERTEXSIZE) 
			{ 
				qglTexCoord2f (v[5] - soffset, v[6] - toffset ); 
				qglVertex3fv (v); 
			}

			qglEnd ();
 
			#ifdef AMIGAOS
			gl_lmaps_drawn++; 
			#endif 
		} 
	} 
} 


#ifdef AMIGAOS

void R_DrawMTexBuffer(void)
{
	GLenum src, dst;

	//GL_TexEnv( GL_REPLACE );
	//qglColor4f(1,1,1,1);

	if ( gl_saturatelighting->value ) 
	{ 
		src = GL_ONE;
		dst = GL_ONE; 
	}
 
	else 
	{ 
		if ( gl_monolightmap->string[0] != '0' ) 
		{ 
			switch ( toupper( gl_monolightmap->string[0] ) ) 
			{ 
				case 'I':
				case 'L': 
					src = GL_ZERO;
					dst = GL_SRC_COLOR; 
					break;
 
				case 'A': 
				default: 
					src = GL_SRC_ALPHA;
					dst = GL_ONE_MINUS_SRC_ALPHA;
					break; 
			} 
		}

		else 
		{ 
			src = GL_ZERO;
			dst = GL_SRC_COLOR; 
		} 
	}

	mglDrawMultitexBuffer(src, dst, GL_REPLACE);
}

#endif

/* 
** R_BlendLightMaps 
** 
** This routine takes all the given light mapped surfaces in the world and 
** blends them into the framebuffer. 
*/
 
void R_BlendLightmaps (void) 
{ 
	int		i; 
	msurface_t	*surf, *newdrawsurf = 0; 
 
	// don't bother if we're set to fullbright 
	if (r_fullbright->value) 
		return;

	if (!r_worldmodel->lightdata) 
		return; 

	#ifdef AMIGAOS

	if( r_vertexlighting->value )
		return;

	if ( mgl_arb_multitexture )
	{
		R_DrawMTexBuffer();
		return;
	}

	#endif

	// don't bother writing Z 
	qglDepthMask( 0 ); 

	/* 
	** set the appropriate blending mode unless we're only looking at the 
	** lightmaps. 
	*/

	if (!gl_lightmap->value) 
	{ 
		qglEnable (GL_BLEND); 
 
		if ( gl_saturatelighting->value ) 
		{ 
			qglBlendFunc( GL_ONE, GL_ONE ); 
		}
 
		else 
		{ 
			if ( gl_monolightmap->string[0] != '0' ) 
			{ 
				switch ( toupper( gl_monolightmap->string[0] ) ) 
				{ 
					case 'I': 
						qglBlendFunc (GL_ZERO, GL_SRC_COLOR ); 
						break;

					case 'L': 
						qglBlendFunc (GL_ZERO, GL_SRC_COLOR ); 
						break;
 
					case 'A': 
					default: 
						qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA ); 
						break; 
				} 
			}

			else 
			{ 
				qglBlendFunc (GL_ZERO, GL_SRC_COLOR ); 
			} 
		} 
	} 

	if ( currentmodel == r_worldmodel ) 
		c_visible_lightmaps = 0; 
 
	/* 
	** render static lightmaps first 
	*/
 
	for ( i = 1; i < MAX_LIGHTMAPS; i++ ) 
	{ 
		if ( gl_lms.lightmap_surfaces[i] ) 
		{ 
			if (currentmodel == r_worldmodel) 
				c_visible_lightmaps++; 

			GL_Bind( gl_state.lightmap_textures + i); 
 
			for ( surf = gl_lms.lightmap_surfaces[i]; surf != 0; surf = surf->lightmapchain ) 
			{ 
				if ( surf->polys ) 
					DrawGLPolyChain( surf->polys, 0, 0 ); 
			} 
		} 
	} 

	/* 
	** render dynamic lightmaps 
	*/
 
	if ( gl_dynamic->value ) 
	{ 
		LM_InitBlock(); 
 
		GL_Bind( gl_state.lightmap_textures+0 ); 
 
		if (currentmodel == r_worldmodel) 
			c_visible_lightmaps++; 
 
		newdrawsurf = gl_lms.lightmap_surfaces[0]; 
 
		for ( surf = gl_lms.lightmap_surfaces[0]; surf != 0; surf = surf->lightmapchain ) 
		{ 
			int	smax, tmax; 
			byte    *base; 
 
			smax = (surf->extents[0]>>4)+1; 
			tmax = (surf->extents[1]>>4)+1; 
 
			if ( LM_AllocBlock( smax, tmax, &surf->dlight_s, &surf->dlight_t ) ) 
			{ 
				base = gl_lms.lightmap_buffer; 
				base += ( surf->dlight_t * BLOCK_WIDTH + surf->dlight_s ) * LIGHTMAP_BYTES; 
 
				R_BuildLightMap (surf, base, BLOCK_WIDTH*LIGHTMAP_BYTES); 
			}
 
			else 
			{ 
				msurface_t *drawsurf; 
 
				// upload what we have so far 
				LM_UploadBlock( true ); 
 
				// draw all surfaces that use this lightmap 
				for ( drawsurf = newdrawsurf; drawsurf != surf; drawsurf = drawsurf->lightmapchain ) 
				{ 
					if ( drawsurf->polys ) 
						DrawGLPolyChain( drawsurf->polys,  
							( drawsurf->light_s - drawsurf->dlight_s ) * ( 1.0 / 128.0 ),  
							( drawsurf->light_t - drawsurf->dlight_t ) * ( 1.0 / 128.0 ) ); 
				} 
 
				newdrawsurf = drawsurf; 
 
				// clear the block 
				LM_InitBlock(); 
 
				// try uploading the block now 
				if ( !LM_AllocBlock( smax, tmax, &surf->dlight_s, &surf->dlight_t ) ) 
				{ 
					ri.Sys_Error( ERR_FATAL, "Consecutive calls to LM_AllocBlock(%d,%d) failed (dynamic)\n", smax, tmax ); 
				} 
 
				base = gl_lms.lightmap_buffer; 
				base += ( surf->dlight_t * BLOCK_WIDTH + surf->dlight_s ) * LIGHTMAP_BYTES; 
 
				R_BuildLightMap (surf, base, BLOCK_WIDTH*LIGHTMAP_BYTES); 
			} 
		} 
 
		/* 
		** draw remainder of dynamic lightmaps that haven't been uploaded yet 
		*/
 
		if ( newdrawsurf ) 
			LM_UploadBlock( true ); 
 
		for ( surf = newdrawsurf; surf != 0; surf = surf->lightmapchain ) 
		{ 
			if ( surf->polys ) 
				DrawGLPolyChain( surf->polys, 
					( surf->light_s - surf->dlight_s ) * ( 1.0 / 128.0 ), 
					( surf->light_t - surf->dlight_t ) * ( 1.0 / 128.0 ) ); 
		} 

	} 
 
	/* 
	** restore state 
	*/ 

	qglDisable (GL_BLEND); 
	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 
	qglDepthMask( 1 ); 
} 

 
/* 
================ 
R_RenderBrushPoly 
================ 
*/

void R_RenderBrushPoly (msurface_t *fa) 
{ 
	int	maps; 
	image_t	*image;
 
	qboolean is_dynamic = false; 

	c_brush_polys++; 
 
	image = R_TextureAnimation (fa->texinfo); 
 
	if (fa->flags & SURF_DRAWTURB) 
	{        
		#ifdef AMIGAOS

		if( mgl_arb_enabled )
		{
			qglActiveTextureARB( GL_TEXTURE1_ARB );
			qglDisable(GL_TEXTURE_2D);
			qglActiveTextureARB( GL_TEXTURE0_ARB );
			mgl_arb_enabled = false;
		}

		#endif

		GL_Bind( image->texnum ); 
 
		// warp texture, no lightmaps 

		#ifdef AMIGAOS
		if( !r_vertexlighting->value )
		#endif
			GL_TexEnv( GL_MODULATE ); 

		qglColor4f( gl_state.inverse_intensity, gl_state.inverse_intensity, gl_state.inverse_intensity, 1.0F ); 
		EmitWaterPolys (fa);
 
		#ifdef AMIGAOS
		if( !r_vertexlighting->value )
		#endif
			GL_TexEnv( GL_REPLACE ); 
 
		return; 
	}
 
	else 
	{ 
		#ifdef AMIGAOS

		if( mgl_arb_multitexture && !r_vertexlighting->value)
		{
		   	if( !mgl_arb_enabled )
		   	{
				GL_Bind( image->texnum ); 
				qglActiveTextureARB(GL_TEXTURE1_ARB);
				qglEnable(GL_TEXTURE_2D);
				GL_Bind( gl_state.lightmap_textures + fa->lightmaptexturenum );

				mgl_arb_enabled = true; 
		   	}

		   	else
		   	{
				qglActiveTextureARB(GL_TEXTURE0_ARB);
				GL_Bind( image->texnum ); 
				qglActiveTextureARB(GL_TEXTURE1_ARB);
				GL_Bind( gl_state.lightmap_textures + fa->lightmaptexturenum );
		   	}		
		}

		else
		{
			GL_Bind( image->texnum );

    			if(!r_vertexlighting->value)
				GL_TexEnv( GL_REPLACE );
		}

		#else

		GL_Bind( image->texnum ); 
		GL_TexEnv( GL_REPLACE );
 
		#endif
	} 
 
//====== 
//PGM

	#ifndef AMIGAOS

	if(fa->texinfo->flags & SURF_FLOWING) 
		DrawGLFlowingPoly (fa);
 
	else 
		DrawGLPoly (fa->polys);
 
	#else

   	if(!r_vertexlighting->value)
   	{
		if(fa->texinfo->flags & SURF_FLOWING) 
			DrawGLFlowingPoly (fa);
 
		else 
			DrawGLPoly (fa->polys);

		if( mgl_arb_multitexture )
			return;
   	}

   	else
   	{
		if(fa->texinfo->flags & SURF_FLOWING) 
			DrawGLFlowingPoly_VLight (fa);
 
		else 
			DrawGLPoly_VLight (fa);

		return;
   	}

	#endif

//PGM 
//====== 

	/* 
	** check for lightmap modification 
	*/
 
	for ( maps = 0; maps < MAXLIGHTMAPS && fa->styles[maps] != 255; maps++ ) 
	{ 
		if ( r_newrefdef.lightstyles[fa->styles[maps]].white != fa->cached_light[maps] ) 
			goto dynamic; 
	} 
 
	// dynamic this frame or dynamic previously 
	if ( ( fa->dlightframe == r_framecount ) ) 
	{ 
dynamic: 
		if ( gl_dynamic->value ) 
		{ 
			if (!( fa->texinfo->flags & (SURF_SKY|SURF_TRANS33|SURF_TRANS66|SURF_WARP ) ) ) 
			{ 
				is_dynamic = true; 
			} 
		} 

	} 
 
	if ( is_dynamic ) 
	{ 
		if ( ( fa->styles[maps] >= 32 || fa->styles[maps] == 0 ) && ( fa->dlightframe != r_framecount ) ) 
		{ 
			unsigned	temp[34*34]; 
			int		smax, tmax; 
 
			smax = (fa->extents[0]>>4)+1; 
			tmax = (fa->extents[1]>>4)+1; 
 
			R_BuildLightMap( fa, (void *)temp, smax*LIGHTMAP_BYTES );
 
			R_SetCacheState( fa ); 
 
			GL_Bind( gl_state.lightmap_textures + fa->lightmaptexturenum ); 
 
			qglTexSubImage2D( GL_TEXTURE_2D, 0, fa->light_s, fa->light_t, smax, tmax, GL_LIGHTMAP_FORMAT, GL_UNSIGNED_BYTE, temp ); 
 
			fa->lightmapchain = gl_lms.lightmap_surfaces[fa->lightmaptexturenum]; 
			gl_lms.lightmap_surfaces[fa->lightmaptexturenum] = fa; 
		}
 
		else 
		{ 
			fa->lightmapchain = gl_lms.lightmap_surfaces[0]; 
			gl_lms.lightmap_surfaces[0] = fa; 
		} 
	}
 
	else 
	{ 
		fa->lightmapchain = gl_lms.lightmap_surfaces[fa->lightmaptexturenum]; 
		gl_lms.lightmap_surfaces[fa->lightmaptexturenum] = fa; 
	} 
} 
 
 
/* 
================ 
R_DrawAlphaSurfaces 
 
Draw water surfaces and windows. 
The BSP tree is waled front to back, so unwinding the chain 
of alpha_surfaces will draw back to front, giving proper ordering. 
================ 
*/

void R_DrawAlphaSurfaces (void) 
{ 
	msurface_t      *s; 
	float           intens; 
 
	// 
	// go back to the world matrix 
	//
	qglLoadMatrixf (r_world_matrix); 
 
	qglEnable (GL_BLEND); 
	GL_TexEnv( GL_MODULATE ); 
 
	// the textures are prescaled up for a better lighting range, 
	// so scale it back down 
	intens = gl_state.inverse_intensity; 
 
	for (s=r_alpha_surfaces ; s ; s=s->texturechain) 
	{ 
		GL_Bind(s->texinfo->image->texnum); 
		c_brush_polys++;
 
		if (s->texinfo->flags & SURF_TRANS33) 
			qglColor4f (intens,intens,intens,0.33);
 
		else if (s->texinfo->flags & SURF_TRANS66) 
			qglColor4f (intens,intens,intens,0.66);
 
		else 
			qglColor4f (intens,intens,intens,1);
 
		if (s->flags & SURF_DRAWTURB) 
			EmitWaterPolys (s); 

		else if(s->texinfo->flags & SURF_FLOWING)	// PGM  9/16/98 
			DrawGLFlowingPoly (s);			// PGM

		else 
			DrawGLPoly (s->polys);
 
	} 
 
	GL_TexEnv( GL_REPLACE ); 
	qglColor4f (1,1,1,1); 
	qglDisable (GL_BLEND); 
 
	r_alpha_surfaces = NULL; 
} 
 
/* 
================ 
DrawTextureChains 
================ 
*/
 
void DrawTextureChains (void) 
{ 
	int             i; 
	msurface_t      *s; 
	image_t         *image; 
 
	c_visible_textures = 0; 
 
	// GL_TexEnv( GL_REPLACE ); 
 
	if ( !arb_multitexture ) 
	{
		#ifdef AMIGAOS

		if( r_vertexlighting->value )
			GL_TexEnv( GL_MODULATE );

		#endif
 
		for ( i = 0, image=gltextures ; i<numgltextures ; i++, image++) 
		{ 
			if (!image->registration_sequence) 
				continue;
 
			s = image->texturechain;
 
			if (!s) 
				continue;

			c_visible_textures++; 

			for ( ; s ; s=s->texturechain) 
				R_RenderBrushPoly (s); 

			image->texturechain = NULL; 
		}
 
		#ifdef AMIGAOS

		if( mgl_arb_enabled )
		{
			qglActiveTextureARB( GL_TEXTURE1_ARB );
			qglDisable(GL_TEXTURE_2D);
			qglActiveTextureARB( GL_TEXTURE0_ARB );
			mgl_arb_enabled = false;
		}

		#endif
	}

	else 
	{ 
		for ( i = 0, image=gltextures ; i<numgltextures ; i++, image++) 
		{ 
			if (!image->registration_sequence) 
				continue;
 
			if (!image->texturechain) 
				continue;
 
			c_visible_textures++; 
 
			for ( s = image->texturechain; s ; s=s->texturechain) 
			{ 
				if ( !( s->flags & SURF_DRAWTURB ) ) 
					R_RenderBrushPoly (s); 
			} 
		} 
 
		GL_EnableMultitexture( false );
 
		for ( i = 0, image=gltextures ; i<numgltextures ; i++, image++) 
		{ 
			if (!image->registration_sequence) 
				continue;
 
			s = image->texturechain;

			if (!s) 
				continue; 
 
			for ( ; s ; s=s->texturechain) 
			{ 
				if ( s->flags & SURF_DRAWTURB ) 
					R_RenderBrushPoly (s); 
			} 
 
			image->texturechain = NULL; 
		}
 
		// GL_EnableMultitexture( true );
	} 
 
	GL_TexEnv( GL_REPLACE ); 
} 

#if 0

static void GL_RenderLightmappedPoly( msurface_t *surf ) 
{ 
	int             i, nv = surf->polys->numverts; 
	int             map; 
	float   	*v;
	image_t  	*image = R_TextureAnimation( surf->texinfo ); 
	qboolean 	is_dynamic = false; 
	unsigned 	lmtex = surf->lightmaptexturenum; 
	glpoly_t 	*p; 
 
	for ( map = 0; map < MAXLIGHTMAPS && surf->styles[map] != 255; map++ ) 
	{ 
		if ( r_newrefdef.lightstyles[surf->styles[map]].white != surf->cached_light[map] ) 
			goto dynamic; 
	} 
 
	// dynamic this frame or dynamic previously 
	if ( ( surf->dlightframe == r_framecount ) ) 
	{ 
dynamic: 
		if ( gl_dynamic->value ) 
		{ 
			if ( !(surf->texinfo->flags & (SURF_SKY|SURF_TRANS33|SURF_TRANS66|SURF_WARP ) ) ) 
			{ 
				is_dynamic = true; 
			} 
		} 
	} 
 
	if ( is_dynamic ) 
	{ 
		unsigned	temp[128*128]; 
		int             smax, tmax; 
 
		if ( ( surf->styles[map] >= 32 || surf->styles[map] == 0 ) && ( surf->dlightframe != r_framecount ) ) 
		{ 
			smax = (surf->extents[0]>>4)+1; 
			tmax = (surf->extents[1]>>4)+1; 
 
			R_BuildLightMap( surf, (void *)temp, smax*LIGHTMAP_BYTES );
			R_SetCacheState( surf ); 
 
			GL_MBind( GL_TEXTURE1_ARB, gl_state.lightmap_textures + surf->lightmaptexturenum ); 
 
			lmtex = surf->lightmaptexturenum; 
 
			qglTexSubImage2D( GL_TEXTURE_2D, 0, surf->light_s, surf->light_t,  smax, tmax, 
				GL_LIGHTMAP_FORMAT, GL_UNSIGNED_BYTE, temp ); 
 
		}
 
		else 
		{ 
			smax = (surf->extents[0]>>4)+1; 
			tmax = (surf->extents[1]>>4)+1; 
 
			R_BuildLightMap( surf, (void *)temp, smax*LIGHTMAP_BYTES );

			GL_MBind( GL_TEXTURE1_ARB, gl_state.lightmap_textures + 0 ); 
 
			lmtex = 0; 
 
			qglTexSubImage2D( GL_TEXTURE_2D, 0, surf->light_s, surf->light_t, smax, tmax,  
				GL_LIGHTMAP_FORMAT, GL_UNSIGNED_BYTE, temp ); 
 
		} 
 
		c_brush_polys++; 
 
		GL_MBind( GL_TEXTURE0_ARB, image->texnum ); 
		GL_MBind( GL_TEXTURE1_ARB, gl_state.lightmap_textures + lmtex ); 
 
//========== 
//PGM 
		if (surf->texinfo->flags & SURF_FLOWING) 
		{ 
			float scroll; 
		 
			scroll = -64 * ( (r_newrefdef.time / 40.0 ) - (int)(r_newrefdef.time / 40.0) );
 
			if(scroll == 0.0) 
				scroll = -64.0;

			for ( p = surf->polys; p; p = p->chain ) 
			{ 
				v = p->verts[0]; 
				qglBegin (GL_POLYGON);
 
				for (i=0 ; i< nv; i++, v+= VERTEXSIZE) 
				{ 
 					qglMultiTexCoord2fARB (GL_TEXTURE0_ARB, (v[3]+scroll), v[4]);
					qglMultiTexCoord2fARB (GL_TEXTURE1_ARB, v[5], v[6]);
					qglVertex3fv (v); 
				}

				qglEnd (); 
			}

		}
 
		else 
		{ 
			for ( p = surf->polys; p; p = p->chain ) 
			{ 
				v = p->verts[0]; 
				qglBegin (GL_POLYGON);
 
				for (i=0 ; i< nv; i++, v+= VERTEXSIZE) 
				{ 
 					qglMultiTexCoord2fARB (GL_TEXTURE0_ARB, (v[3]), v[4]);
					qglMultiTexCoord2fARB (GL_TEXTURE1_ARB, v[5], v[6]);
					qglVertex3fv (v); 
				}

				qglEnd (); 
			} 
		} 
//PGM 
//========== 
	}

	else 
	{ 
		c_brush_polys++; 
 
		GL_MBind( GL_TEXTURE0_ARB, image->texnum ); 
		GL_MBind( GL_TEXTURE1_ARB, gl_state.lightmap_textures + lmtex ); 
 
//========== 
//PGM 
		if (surf->texinfo->flags & SURF_FLOWING) 
		{ 
			float scroll; 
		 
			scroll = -64 * ( (r_newrefdef.time / 40.0 ) - (int)(r_newrefdef.time / 40.0 ) );
 
			if(scroll == 0.0) 
				scroll = -64.0;

			for ( p = surf->polys; p; p = p->chain ) 
			{ 
				v = p->verts[0]; 
				qglBegin (GL_POLYGON); 

				for (i=0 ; i< nv; i++, v+= VERTEXSIZE) 
				{ 
 					qglMultiTexCoord2fARB (GL_TEXTURE0_ARB, (v[3]+scroll), v[4]);
					qglMultiTexCoord2fARB (GL_TEXTURE1_ARB, v[5], v[6]);
					qglVertex3fv (v); 
				}
 
				qglEnd (); 
			}

		}
 
		else 
		{
//PGM 
//========== 
			for ( p = surf->polys; p; p = p->chain ) 
			{ 
				v = p->verts[0]; 
				qglBegin (GL_POLYGON);
 
				for (i=0 ; i< nv; i++, v+= VERTEXSIZE) 
				{ 
 					qglMultiTexCoord2fARB (GL_TEXTURE0_ARB, (v[3]+scroll), v[4]);
					qglMultiTexCoord2fARB (GL_TEXTURE1_ARB, v[5], v[6]);
					qglVertex3fv (v); 
				}

				qglEnd (); 
			} 
//========== 
//PGM 
		} 
//PGM 
//========== 
	} 
}
 
#else

static void GL_RenderLightmappedPoly( msurface_t *surf ) 
{ 
	int             i, nv = surf->polys->numverts; 
	int             map; 
	float   	*v; 
	image_t  	*image = R_TextureAnimation( surf->texinfo ); 
	qboolean 	is_dynamic = false; 
	unsigned 	lmtex = surf->lightmaptexturenum; 
	glpoly_t 	*p; 
 
	for ( map = 0; map < MAXLIGHTMAPS && surf->styles[map] != 255; map++ ) 
	{ 
		if ( r_newrefdef.lightstyles[surf->styles[map]].white != surf->cached_light[map] ) 
			goto dynamic; 
	} 
 
	// dynamic this frame or dynamic previously 
	if ( ( surf->dlightframe == r_framecount ) ) 
	{ 
dynamic: 
		if ( gl_dynamic->value ) 
		{ 
			if ( !(surf->texinfo->flags & (SURF_SKY|SURF_TRANS33|SURF_TRANS66|SURF_WARP ) ) ) 
			{ 
				is_dynamic = true; 
			} 
		} 
	} 
 
	if ( is_dynamic ) 
	{ 
		unsigned	temp[128*128]; 
		int             smax, tmax; 
 
		smax = (surf->extents[0]>>4)+1; 
		tmax = (surf->extents[1]>>4)+1; 
 
		R_BuildLightMap( surf, (void *)temp, smax*LIGHTMAP_BYTES );

		if ( ( surf->styles[map] >= 32 || surf->styles[map] == 0 ) && ( surf->dlightframe != r_framecount ) ) 
		{ 		
			R_SetCacheState( surf ); 
			GL_MBind( GL_TEXTURE1_ARB, gl_state.lightmap_textures + surf->lightmaptexturenum ); 
			lmtex = surf->lightmaptexturenum; 
		}

		else 
		{ 			
			GL_MBind( GL_TEXTURE1_ARB, gl_state.lightmap_textures + 0 ); 
			lmtex = 0; 
		}

		qglTexSubImage2D( GL_TEXTURE_2D, 0, surf->light_s, surf->light_t, smax, tmax, GL_LIGHTMAP_FORMAT, GL_UNSIGNED_BYTE, temp );
	} 
 
	c_brush_polys++; 
 
	GL_MBind( GL_TEXTURE0_ARB, image->texnum ); 
	GL_MBind( GL_TEXTURE1_ARB, gl_state.lightmap_textures + lmtex ); 
 
	if (surf->texinfo->flags & SURF_FLOWING) 
	{
		float scroll;

		scroll = -64 * ( (r_newrefdef.time / 40.0 ) - (int)(r_newrefdef.time / 40.0 ) );
 
		if(scroll == 0.0) 
			scroll = -64.0;

		for ( p = surf->polys; p; p = p->chain ) 
		{ 
			v = p->verts[0]; 
			qglBegin (GL_POLYGON);

			for (i=0 ; i< nv; i++, v+= VERTEXSIZE) 
			{ 
 				qglMultiTexCoord2fARB (GL_TEXTURE0_ARB, (v[3]+scroll), v[4]);
				qglMultiTexCoord2fARB (GL_TEXTURE1_ARB, v[5], v[6]);
				qglVertex3fv (v); 
			}

			qglEnd (); 
		}
	}

	else
	{
		for ( p = surf->polys; p; p = p->chain ) 
		{ 
			v = p->verts[0]; 
			qglBegin (GL_POLYGON);

			for (i=0 ; i< nv; i++, v+= VERTEXSIZE) 
			{ 
 				qglMultiTexCoord2fARB (GL_TEXTURE0_ARB, v[3], v[4]);
				qglMultiTexCoord2fARB (GL_TEXTURE1_ARB, v[5], v[6]);
				qglVertex3fv (v); 
			}

			qglEnd (); 
		}
	}
} 

#endif
 
/* 
================= 
R_DrawInlineBModel 
================= 
*/
 
void R_DrawInlineBModel (void) 
{ 
	int             i, k; 
	cplane_t        *pplane; 
	float           dot; 
	msurface_t      *psurf, *s; 
	dlight_t        *lt; 

	qboolean	duplicate;
 
	// calculate dynamic lighting for bmodel

	#ifndef AMIGAOS
	if ( !gl_flashblend->value )
	#else
	if ( gl_flashblend->value != 1)
	#endif 
	{ 
		lt = r_newrefdef.dlights; 

		for (k=0 ; k<r_newrefdef.num_dlights ; k++, lt++) 
		{ 
			R_MarkLights (lt, 1<<k, currentmodel->nodes + currentmodel->firstnode); 
		} 
		
	} 
 
	psurf = &currentmodel->surfaces[currentmodel->firstmodelsurface]; 
 
	if ( currententity->flags & RF_TRANSLUCENT ) 
	{
		qglColor4f (1, 1, 1, 0.25);

		#ifndef AMIGAOS //moved because of entity state-sorting 
		GL_TexEnv( GL_MODULATE ); 
		qglEnable (GL_BLEND);
		#endif 
	}

	// 
	// draw texture 
	//

	for (i=0 ; i<currentmodel->nummodelsurfaces ; i++, psurf++)
	{ 
		// find which side of the node we are on 
		pplane = psurf->plane; 
 
		dot = DotProduct (modelorg, pplane->normal) - pplane->dist; 
 
		// draw the polygon 
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) || (!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON))) 
		{ 
			if (psurf->texinfo->flags & (SURF_TRANS33|SURF_TRANS66) ) 
			{
			      	// add to the translucent chain 
				psurf->texturechain = r_alpha_surfaces; 
				r_alpha_surfaces = psurf;
			}

			else if ( arb_multitexture && !( psurf->flags & SURF_DRAWTURB ) ) 
			{ 
				GL_RenderLightmappedPoly( psurf ); 
			}

			else 
			{ 
				#ifdef AMIGAOS

				if(r_vertexlighting->value && !( currententity->flags & RF_TRANSLUCENT ))
					GL_TexEnv( GL_MODULATE );
 
				#endif

				GL_EnableMultitexture( false ); 
				R_RenderBrushPoly( psurf );
				GL_EnableMultitexture( true ); 
			} 
		} 
	} 

	#ifdef AMIGAOS

	if( mgl_arb_enabled )
	{
		qglActiveTextureARB( GL_TEXTURE1_ARB );
		qglDisable(GL_TEXTURE_2D);
		qglActiveTextureARB( GL_TEXTURE0_ARB );
		mgl_arb_enabled = false;
	}

	#endif
 
	if ( !(currententity->flags & RF_TRANSLUCENT) ) 
	{
		if ( !arb_multitexture ) 
			R_BlendLightmaps (); 
		
		#ifdef AMIGAOS

		if(r_vertexlighting->value)
		{
			qglColor4f (1, 1, 1, 1); 
			GL_TexEnv( GL_REPLACE );
		}

		#endif
	}
 
	else 
	{ 
		#ifndef AMIGAOS //moved because of entity state-sorting
		qglDisable (GL_BLEND); 
		GL_TexEnv( GL_REPLACE );
		#endif

		qglColor4f (1, 1, 1, 1); 
	} 
} 


/* 
================= 
R_DrawBrushModel 
================= 
*/

void R_DrawBrushModel (entity_t *e) 
{
	vec3_t          mins, maxs; 
	int             i; 
	qboolean        rotated; 
 
	if (currentmodel->nummodelsurfaces == 0) 
		return; 
 
	currententity = e; 
	gl_state.currenttextures[0] = gl_state.currenttextures[1] = -1; 
 
	if (e->angles[0] || e->angles[1] || e->angles[2]) 
	{ 
		rotated = true;
 
		for (i=0 ; i<3 ; i++) 
		{ 
			mins[i] = e->origin[i] - currentmodel->radius; 
			maxs[i] = e->origin[i] + currentmodel->radius; 
		} 
	}
 
	else 
	{ 
		rotated = false; 
		VectorAdd (e->origin, currentmodel->mins, mins); 
		VectorAdd (e->origin, currentmodel->maxs, maxs); 
	} 
 
	if (R_CullBox (mins, maxs)) 
		return; 

	qglColor3f (1,1,1); 
	memset (gl_lms.lightmap_surfaces, 0, sizeof(gl_lms.lightmap_surfaces)); 
 
	VectorSubtract (r_newrefdef.vieworg, e->origin, modelorg);
 
	if (rotated) 
	{
		//ri.Con_Printf (PRINT_ALL, "rotated\n");
		vec3_t  temp; 
		vec3_t  forward, right, up; 
 
		VectorCopy (modelorg, temp); 
		AngleVectors (e->angles, forward, right, up); 
		modelorg[0] = DotProduct (temp, forward); 
		modelorg[1] = -DotProduct (temp, right); 
		modelorg[2] = DotProduct (temp, up); 
	} 
 
    	qglPushMatrix ();

	#if 0

	e->angles[0] = -e->angles[0];   // stupid quake bug 
	e->angles[2] = -e->angles[2];   // stupid quake bug 
	R_RotateForEntity (e); 
	e->angles[0] = -e->angles[0];   // stupid quake bug 
	e->angles[2] = -e->angles[2];   // stupid quake bug

	#else // Cowcat

	qglTranslatef (e->origin[0],  e->origin[1],  e->origin[2]);
	
	if(rotated)
	{
		glRotatefEXT (e->angles[1], GLROT_001); 
    		glRotatefEXT (e->angles[0], GLROT_010); 
    		glRotatefEXT (e->angles[2], GLROT_100);
	}

	#endif
 
	if( arb_multitexture ) //surgeon
	{
		GL_EnableMultitexture( true ); 
		GL_SelectTexture( GL_TEXTURE0_ARB); 
		GL_TexEnv( GL_REPLACE ); 
		GL_SelectTexture( GL_TEXTURE1_ARB); 
		GL_TexEnv( GL_MODULATE );
	}
 
	R_DrawInlineBModel ();

	GL_EnableMultitexture( false ); 

	#ifdef AMIGAOS

	if( mgl_arb_enabled )
	{
		qglActiveTextureARB( GL_TEXTURE1_ARB );
		qglDisable(GL_TEXTURE_2D);
		qglActiveTextureARB( GL_TEXTURE0_ARB );
		mgl_arb_enabled = false;
	}

	#endif
 
	qglPopMatrix (); 
} 
 
/* 
============================================================= 
 
	WORLD MODEL 
 
============================================================= 
*/ 

 
/* 
================ 
R_RecursiveWorldNode 
================ 
*/

//#define NEW_CULLING_STYLE

#ifdef NEW_CULLING_STYLE
void R_RecursiveWorldNode (mnode_t *node, int clipflags)
#else
void R_RecursiveWorldNode (mnode_t *node)
#endif
{ 
	int             c, side, sidebit; 
	cplane_t        *plane; 
	msurface_t      *surf, **mark; 
	mleaf_t         *pleaf; 
	float           dot; 
	image_t         *image; 

loc0: //surgeon

	if (node->contents == CONTENTS_SOLID) 
		return;         // solid 
 
	if (node->visframe != r_visframecount) 
		return;

	#ifndef NEW_CULLING_STYLE

	if (R_CullBox (node->minmaxs, node->minmaxs+3)) 
		return;

	#else

	int i, ret;

	for (i = 0; i < 4; i++)
	{
		if (!(clipflags & (1 << i)))
			continue;

		ret = BoxOnPlaneSide (node->minmaxs, node->minmaxs+3, &frustum[i]);

		if (ret == 2)
			return;

		else if (ret == 1)
			clipflags &=~(1 << i);
	}

	#endif

	// if a leaf node, draw stuff 
	if (node->contents != -1) 
	{ 
		pleaf = (mleaf_t *)node; 
 
		// check for door connected areas 
		if (r_newrefdef.areabits) 
		{ 
			if (! (r_newrefdef.areabits[pleaf->area>>3] & (1<<(pleaf->area&7)) ) ) 
				return;         // not visible 
		} 
 
		mark = pleaf->firstmarksurface; 
		c = pleaf->nummarksurfaces; 
 
		if (c) 
		{ 
			do 
			{ 
				(*mark)->visframe = r_framecount; 
				mark++;
 
			} while (--c); 
		} 
 
		return; 
	} 
 
	// node is just a decision point, so go down the apropriate sides 
 
	// find which side of the node we are on 
	plane = node->plane; 

	switch (plane->type) 
	{ 
		case PLANE_X: 
			dot = modelorg[0] - plane->dist; 
			break;
 
		case PLANE_Y: 
			dot = modelorg[1] - plane->dist; 
			break;
 
		case PLANE_Z: 
			dot = modelorg[2] - plane->dist; 
			break;
 
		default: 
			dot = DotProduct (modelorg, plane->normal) - plane->dist; 
			break; 
	} 
		 
	if (dot >= 0) 
	{ 
		side = 0; 
		sidebit = 0; 
	}

	else 
	{ 
		side = 1; 
		sidebit = SURF_PLANEBACK; 
	} 
 
	// recurse down the children, front side first
	#ifdef NEW_CULLING_STYLE
	R_RecursiveWorldNode (node->children[side], clipflags);
	#else 
	R_RecursiveWorldNode (node->children[side]);
	#endif

	// draw stuff 
	for ( c = node->numsurfaces, surf = r_worldmodel->surfaces + node->firstsurface; c ; c--, surf++) 
	{
		if (surf->visframe != r_framecount) 
			continue; 
 
		if ( (surf->flags & SURF_PLANEBACK) != sidebit ) 
			continue;               // wrong side 

		if (surf->texinfo->flags & SURF_SKY) 
		{
		       // just adds to visible sky bounds 
			R_AddSkySurface (surf); 
		}

		else if (surf->texinfo->flags & (SURF_TRANS33|SURF_TRANS66)) 
		{
		       // add to the translucent chain 
			surf->texturechain = r_alpha_surfaces; 
			r_alpha_surfaces = surf;
			r_alpha_surfaces->texinfo->image = R_TextureAnimation(surf->texinfo); //fix scroll display animation -Cowcat
		}

		else 
		{ 
			if ( arb_multitexture && !( surf->flags & SURF_DRAWTURB ) ) 
			{ 
				GL_RenderLightmappedPoly( surf ); 
			}
 
			else 
			{ 
				// the polygon is visible, so add it to the texture 
				// sorted chain 
				// FIXME: this is a hack for animation 
				image = R_TextureAnimation (surf->texinfo); 
				surf->texturechain = image->texturechain; 
				image->texturechain = surf; 
			} 
		} 
	} 
 
	// recurse down the back side

	#if 0

	R_RecursiveWorldNode (node->children[!side]);

	#else //surgeon: optimized recursion

	node = node->children[!side];
	goto loc0;

	#endif

} 
 
/* 
============= 
R_DrawWorld 
============= 
*/
 
void R_DrawWorld (void) 
{ 
	entity_t ent;

	//int	starttest, endtest; //

	if (!r_drawworld->value) 
		return; 
 
	if ( r_newrefdef.rdflags & RDF_NOWORLDMODEL ) 
		return; 
 
	currentmodel = r_worldmodel; 
 
	VectorCopy (r_newrefdef.vieworg, modelorg); 
 
	// auto cycle the world frame for texture animation 
	memset (&ent, 0, sizeof(ent)); 
	ent.frame = (int)(r_newrefdef.time*2); 
	currententity = &ent; 
 
	gl_state.currenttextures[0] = gl_state.currenttextures[1] = -1; 
 
	qglColor4f (1,1,1,1);
 
	memset (gl_lms.lightmap_surfaces, 0, sizeof(gl_lms.lightmap_surfaces)); 
	R_ClearSkyBox (); 

	//starttest = Sys_Milliseconds();

	if ( arb_multitexture ) 
	{ 
		GL_EnableMultitexture( true ); 
 
		GL_SelectTexture( GL_TEXTURE0_ARB); 
		GL_TexEnv( GL_REPLACE ); 
		GL_SelectTexture( GL_TEXTURE1_ARB); 
 
		if ( gl_lightmap->value ) 
			GL_TexEnv( GL_REPLACE );
 
		else  
			GL_TexEnv( GL_MODULATE ); 
 
		#ifdef NEW_CULLING_STYLE
		R_RecursiveWorldNode (r_worldmodel->nodes, 15);
		#else
		R_RecursiveWorldNode (r_worldmodel->nodes);
		#endif 
 
		GL_EnableMultitexture( false ); 
	}

	else 
	{
		#ifdef NEW_CULLING_STYLE
		R_RecursiveWorldNode (r_worldmodel->nodes, 15);
		#else
		R_RecursiveWorldNode (r_worldmodel->nodes);
		#endif
	} 

	/* 
	** theoretically nothing should happen in the next two functions 
	** if multitexture is enabled 
	*/ 

	DrawTextureChains (); 

	R_BlendLightmaps (); 
 
	R_DrawSkyBox (); 

	if (gl_showtris->value) 
		R_DrawTriangleOutlines ();

} 
 
/* 
=============== 
R_MarkLeaves 
 
Mark the leaves and nodes that are in the PVS for the current 
cluster 
=============== 
*/

void R_MarkLeaves (void) 
{ 
	byte    	*vis; 
	byte    	fatvis[MAX_MAP_LEAFS/8]; 
	mnode_t 	*node; 
	int             i, c; 
	mleaf_t 	*leaf; 
	int             cluster; 
 
	if (r_oldviewcluster == r_viewcluster && r_oldviewcluster2 == r_viewcluster2 && !r_novis->value && r_viewcluster != -1) 
		return; 
 
	// development aid to let you run around and see exactly where 
	// the pvs ends 
	if (gl_lockpvs->value) 
		return; 
 
	r_visframecount++; 
	r_oldviewcluster = r_viewcluster; 
	r_oldviewcluster2 = r_viewcluster2; 
 
	if (r_novis->value || r_viewcluster == -1 || !r_worldmodel->vis) 
	{ 
		// mark everything 
		for (i=0 ; i<r_worldmodel->numleafs ; i++) 
			r_worldmodel->leafs[i].visframe = r_visframecount;

		for (i=0 ; i<r_worldmodel->numnodes ; i++) 
			r_worldmodel->nodes[i].visframe = r_visframecount;

		return; 
	} 
 
	vis = Mod_ClusterPVS (r_viewcluster, r_worldmodel);

	// may have to combine two clusters because of solid water boundaries 
	if (r_viewcluster2 != r_viewcluster) 
	{ 
		memcpy (fatvis, vis, (r_worldmodel->numleafs+7)/8); 
		vis = Mod_ClusterPVS (r_viewcluster2, r_worldmodel); 
		c = (r_worldmodel->numleafs+31)/32;
 
		for (i=0 ; i<c ; i++) 
			((int *)fatvis)[i] |= ((int *)vis)[i];
 
		vis = fatvis; 
	} 
	 
	for (i=0,leaf=r_worldmodel->leafs ; i<r_worldmodel->numleafs ; i++, leaf++) 
	{ 
		cluster = leaf->cluster;
 
		if (cluster == -1) 
			continue;
 
		if (vis[cluster>>3] & (1<<(cluster&7))) 
		{ 
			node = (mnode_t *)leaf;
 
			do 
			{ 
				if (node->visframe == r_visframecount) 
					break;
 
				node->visframe = r_visframecount; 
				node = node->parent;
 
			} while (node); 
		} 
	} 
} 
 
 
 
/* 
============================================================================= 
 
  LIGHTMAP ALLOCATION 
 
============================================================================= 
*/ 

#ifdef AMIGAOS //vertexlighting
void GL_CreateVertexColors(msurface_t *fa);
#endif
 
static void LM_InitBlock( void ) 
{ 
	memset( gl_lms.allocated, 0, sizeof( gl_lms.allocated ) ); 
} 
 
static void LM_UploadBlock( qboolean dynamic ) 
{ 
	int texture; 
	int height = 0; 
 
	if ( dynamic ) 
	{ 
		texture = 0; 
	}

	else 
	{ 
		texture = gl_lms.current_lightmap_texture; 
	} 
 
	GL_Bind( gl_state.lightmap_textures + texture );
 
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); 
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); 
 
	if ( dynamic ) 
	{ 
		int i; 
 
		for ( i = 0; i < BLOCK_WIDTH; i++ ) 
		{ 
			if ( gl_lms.allocated[i] > height ) 
				height = gl_lms.allocated[i]; 
		} 
 
		qglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, BLOCK_WIDTH, height, GL_LIGHTMAP_FORMAT, GL_UNSIGNED_BYTE, gl_lms.lightmap_buffer ); 
	}

	else 
	{
		#ifdef AMIGAOS //surgeon: create vertexlighting

		int i;

		for(i=0; i < numvlightsurfs; i++)
			GL_CreateVertexColors( vlightsurfs[i] );

		numvlightsurfs = 0;

		#endif

		#ifdef AMIGA_ARGB_4444
 
		qglTexImage2D( GL_TEXTURE_2D, 0, GL_LIGHTMAP_FORMAT, BLOCK_WIDTH, BLOCK_HEIGHT, 0, GL_LIGHTMAP_FORMAT, 
				GL_UNSIGNED_BYTE, gl_lms.lightmap_buffer );
		#else

		qglTexImage2D( GL_TEXTURE_2D, 0, gl_lms.internal_format, BLOCK_WIDTH, BLOCK_HEIGHT, 0, GL_LIGHTMAP_FORMAT,  
				GL_UNSIGNED_BYTE, gl_lms.lightmap_buffer );

		#endif

		if ( ++gl_lms.current_lightmap_texture == MAX_LIGHTMAPS ) 
			ri.Sys_Error( ERR_DROP, "LM_UploadBlock() - MAX_LIGHTMAPS exceeded\n" ); 
	} 
} 
 
// returns a texture number and the position inside it 
static qboolean LM_AllocBlock (int w, int h, int *x, int *y)
{ 
	int	i, j; 
	int	best, best2; 
 
	best = BLOCK_HEIGHT; 
 
	for (i=0 ; i<BLOCK_WIDTH-w ; i++) 
	{ 
		best2 = 0; 
 
		for (j=0 ; j<w ; j++) 
		{ 
			if (gl_lms.allocated[i+j] >= best) 
				break;
 
			if (gl_lms.allocated[i+j] > best2) 
				best2 = gl_lms.allocated[i+j]; 
		}

		if (j == w) 
		{
		       // this is a valid spot 
			*x = i; 
			*y = best = best2; 
		} 
	} 
 
	if (best + h > BLOCK_HEIGHT) 
		return false; 
 
	for (i=0 ; i<w ; i++) 
		gl_lms.allocated[*x + i] = best + h; 
 
	return true; 
} 

 
/* 
================ 
GL_BuildPolygonFromSurface 
================ 
*/ 

void GL_BuildPolygonFromSurface(msurface_t *fa) 
{ 
	int             i, lindex, lnumverts; 
	medge_t         *pedges, *r_pedge; 
	int             vertpage; 
	float           *vec; 
	float           s, t; 
	glpoly_t        *poly; 
	vec3_t          total; 

	#ifdef AMIGAOS

	int nColinElim;
	extern cvar_t *gl_keeptjunctions;

	#endif
 
	// reconstruct the polygon 
	pedges = currentmodel->edges; 
	lnumverts = fa->numedges; 
	vertpage = 0; 
 
	VectorClear (total);
 
	// 
	// draw texture 
	// 

	poly = Hunk_Alloc (sizeof(glpoly_t) + (lnumverts-4) * VERTEXSIZE*sizeof(float)); 
	poly->next = fa->polys; 
	poly->flags = fa->flags; 
	fa->polys = poly; 
	poly->numverts = lnumverts; 
 
	for (i=0 ; i<lnumverts ; i++) 
	{ 
		lindex = currentmodel->surfedges[fa->firstedge + i]; 
 
		if (lindex > 0) 
		{ 
			r_pedge = &pedges[lindex]; 
			vec = currentmodel->vertexes[r_pedge->v[0]].position; 
		}
 
		else 
		{ 
			r_pedge = &pedges[-lindex]; 
			vec = currentmodel->vertexes[r_pedge->v[1]].position; 
		}

		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3]; 
		s /= fa->texinfo->image->width; 
 
		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3]; 
		t /= fa->texinfo->image->height; 
 
		VectorAdd (total, vec, total); 
		VectorCopy (vec, poly->verts[i]); 
		poly->verts[i][3] = s; 
		poly->verts[i][4] = t; 
 
		// 
		// lightmap texture coordinates 
		// 
		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3]; 
		s -= fa->texturemins[0]; 
		s += fa->light_s*16; 
		s += 8; 
		s /= BLOCK_WIDTH*16; //fa->texinfo->texture->width; 
 
		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3]; 
		t -= fa->texturemins[1]; 
		t += fa->light_t*16; 
		t += 8; 
		t /= BLOCK_HEIGHT*16; //fa->texinfo->texture->height; 
 
		poly->verts[i][5] = s; 
		poly->verts[i][6] = t; 
	} 
 
	poly->numverts = lnumverts; 

	#ifdef AMIGAOS // from glquake - colinear vertex removal

  	//
  	// remove co-linear points - Ed
  	//

  	if (!gl_keeptjunctions->value)
  	{
    		for (i = 0 ; i < lnumverts ; ++i)
    		{
      			vec3_t v1, v2;
      			float *prev, *this, *next;
      			float f;

      			prev = poly->verts[(i + lnumverts - 1) % lnumverts];
      			this = poly->verts[i];
      			next = poly->verts[(i + 1) % lnumverts];

      			VectorSubtract( this, prev, v1 );
      			VectorNormalize( v1 );
      			VectorSubtract( next, prev, v2 );
      			VectorNormalize( v2 );

      			// skip co-linear points
      			#define COLINEAR_EPSILON 0.001

      			if ((fabs( v1[0] - v2[0] ) <= COLINEAR_EPSILON) &&
        			(fabs( v1[1] - v2[1] ) <= COLINEAR_EPSILON) && 
        			(fabs( v1[2] - v2[2] ) <= COLINEAR_EPSILON))
      			{
        			int j;

        			for (j = i + 1; j < lnumverts; ++j)
        			{
          				int k;

          				for (k = 0; k < VERTEXSIZE; ++k)
            					poly->verts[j - 1][k] = poly->verts[j][k];
        			}

        			--lnumverts;
        			++nColinElim;
        			// retry next vertex next time, which is now current vertex
        			--i;
      			}
    		}
  	}

  	poly->numverts = lnumverts;

	#endif 
} 

#ifdef AMIGAOS

#ifdef SAMPLE_POLYEDGES

static GLuint ReadEdgeColor (float *ca, float *cb)
{
	//intended for extra color-sampling
	//FIXME: edge-samples are not used at the moment

	GLuint res;
	float edge[2];
	float s1,s2,t1,t2;

	s1 = ca[0];
	t1 = ca[1];
	s2 = cb[0];
	t2 = cb[1];

	if(s2 > s1)	edge[0] = s1 + (s2-s1)*0.5f;
	else		edge[0] = s2 + (s1-s2)*0.5f;

	if(t2 > t1)	edge[1] = t1 + (t2-t1)*0.5f;
	else		edge[1] = t2 + (t1-t2)*0.5f;

	res = *((GLuint *)ReadVertexColor(&edge[0], (GLubyte *)gl_lms.lightmap_buffer));

	return res;
} 

//surgeon: generate vertexcolors from static lightmaps

void GL_CreateVertexColors(msurface_t *fa)
{
	int 		i;
	glpoly_t 	*p;
	GLubyte 	*vertexcolor;
	GLuint 		*dest;
	float 		*v1, *v2;
	GLuint 		vcolor[256]; //vertex color
	GLuint 		ecolor[256]; //edge color

	p = fa->polys;
	v1 = p->verts[0];

	if(fa->lightmaptexturenum >= 1 && fa->samples)
	{
 	   	v2 = p->verts[1];

	   	vcolor[0] =  *((GLuint *)ReadVertexColor(&v1[5], (GLubyte *)gl_lms.lightmap_buffer));
		
	   	for (i=1 ; i<p->numverts; i++, v1+=VERTEXSIZE, v2+=VERTEXSIZE) 
	   	{
			vcolor[i] = *((GLuint *)ReadVertexColor(&v2[5], (GLubyte *)gl_lms.lightmap_buffer));
			ecolor[i-1] = ReadEdgeColor(&v1[5], &v2[5]);

	   	}

	   	v1 = p->verts[0];

	   	ecolor[p->numverts-1] = ReadEdgeColor(&v1[5], &v2[5]);

	   	// store the colors
	   	// (no interpolation with edge samples yet)

	   	for (i=0 ; i<p->numverts ; i++, v1+=VERTEXSIZE) 
	  	{
			dest = (GLuint *)&(v1[7]);
			*dest =  vcolor[i];
	   	}

	}

	else
	{
	   	for (i=0 ; i<p->numverts ; i++, v1+=VERTEXSIZE) 
	   	{
			dest = (GLuint *)&(v1[7]);
			*dest = 0xffffffff;
	   	}
	}
}

#else

//surgeon: generate vertexcolors from static lightmaps

void GL_CreateVertexColors(msurface_t *fa)
{
	int 		i;
	glpoly_t 	*p;
	GLubyte 	*vertexcolor;
	GLuint 		*dest;
	float 		*v;

	p = fa->polys;
	v = p->verts[0];

	if(fa->lightmaptexturenum >= 1 && fa->samples)
	{
	   	for (i=0 ; i<p->numverts; i++, v+=VERTEXSIZE) 
	   	{
			dest = (GLuint *)&(v[7]);
			*dest = *((GLuint *)ReadVertexColor(&v[5], (GLubyte *)gl_lms.lightmap_buffer));
	   	}
	}

	else
	{
	   	for (i=0 ; i<p->numverts ; i++, v+=VERTEXSIZE) 
	   	{
			dest = (GLuint *)&(v[7]);
			*dest = 0xffffffff;
	   	}
	}
}

#endif

#endif
 
/* 
======================== 
GL_CreateSurfaceLightmap 
======================== 
*/
 
void GL_CreateSurfaceLightmap (msurface_t *surf) 
{ 
	int     smax, tmax; 
	byte    *base; 
 
	if (surf->flags & (SURF_DRAWSKY|SURF_DRAWTURB)) 
		return; 
 
	smax = (surf->extents[0]>>4)+1; 
	tmax = (surf->extents[1]>>4)+1; 
 
	if ( !LM_AllocBlock( smax, tmax, &surf->light_s, &surf->light_t ) ) 
	{ 
		LM_UploadBlock( false ); 
		LM_InitBlock();

		if ( !LM_AllocBlock( smax, tmax, &surf->light_s, &surf->light_t ) ) 
		{ 
			ri.Sys_Error( ERR_FATAL, "Consecutive calls to LM_AllocBlock(%d,%d) failed\n", smax, tmax ); 
		} 
	} 
 
	surf->lightmaptexturenum = gl_lms.current_lightmap_texture; 
 
	base = gl_lms.lightmap_buffer; 
	base += (surf->light_t * BLOCK_WIDTH + surf->light_s) * LIGHTMAP_BYTES; 
 
	R_SetCacheState( surf ); 
	R_BuildLightMap (surf, base, BLOCK_WIDTH*LIGHTMAP_BYTES); 

	#ifdef AMIGAOS //surgeon: add surf to vertexlighting
	vlightsurfs[numvlightsurfs++] = surf;
	#endif
} 
 
/* 
================== 
GL_BeginBuildingLightmaps 
 
================== 
*/
 
void GL_BeginBuildingLightmaps (model_t *m) 
{ 
	static lightstyle_t     lightstyles[MAX_LIGHTSTYLES]; 
	int                     i; 
	unsigned                dummy[128*128]; 
 
	memset( gl_lms.allocated, 0, sizeof(gl_lms.allocated) ); 
 
	r_framecount = 1;               // no dlightcache 
 
	GL_EnableMultitexture( true ); 
	GL_SelectTexture( GL_TEXTURE1_ARB); 
 
	/* 
	** setup the base lightstyles so the lightmaps won't have to be regenerated 
	** the first time they're seen 
	*/

	for (i=0 ; i<MAX_LIGHTSTYLES ; i++) 
	{ 
		lightstyles[i].rgb[0] = 1; 
		lightstyles[i].rgb[1] = 1; 
		lightstyles[i].rgb[2] = 1; 
		lightstyles[i].white = 3; 
	}

	r_newrefdef.lightstyles = lightstyles; 
 
	if (!gl_state.lightmap_textures) 
	{ 
		gl_state.lightmap_textures = TEXNUM_LIGHTMAPS; 
		// gl_state.lightmap_textures = gl_state.texture_extension_number; 
		// gl_state.texture_extension_number = gl_state.lightmap_textures + MAX_LIGHTMAPS; 
	} 
 
	gl_lms.current_lightmap_texture = 1; 
 
	/* 
	** if mono lightmaps are enabled and we want to use alpha 
	** blending (a,1-a) then we're likely running on a 3DLabs 
	** Permedia2.  In a perfect world we'd use a GL_ALPHA lightmap 
	** in order to conserve space and maximize bandwidth, however  
	** this isn't a perfect world. 
	** 
	** So we have to use alpha lightmaps, but stored in GL_RGBA format, 
	** which means we only get 1/16th the color resolution we should when 
	** using alpha lightmaps.  If we find another board that supports 
	** only alpha lightmaps but that can at least support the GL_ALPHA 
	** format then we should change this code to use real alpha maps. 
	*/

	if ( toupper( gl_monolightmap->string[0] ) == 'A' ) 
	{ 
		gl_lms.internal_format = gl_tex_alpha_format; 
	}

	/* 
	** try to do hacked colored lighting with a blended texture 
	*/
 
	else if ( toupper( gl_monolightmap->string[0] ) == 'C' ) 
	{ 
		gl_lms.internal_format = gl_tex_alpha_format; 
	}

	else if ( toupper( gl_monolightmap->string[0] ) == 'I' ) 
	{ 
		gl_lms.internal_format = GL_INTENSITY8; 
	}
 
	else if ( toupper( gl_monolightmap->string[0] ) == 'L' )  
	{ 
		gl_lms.internal_format = GL_LUMINANCE8; 
	}
 
	else 
	{ 
		gl_lms.internal_format = gl_tex_solid_format; 
	} 
 
	/* 
	** initialize the dynamic lightmap texture 
	*/ 

	GL_Bind( gl_state.lightmap_textures + 0 );
 
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); 
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
 
	#ifdef AMIGA_ARGB_4444

	qglTexImage2D(GL_TEXTURE_2D,0,GL_LIGHTMAP_FORMAT,BLOCK_WIDTH,BLOCK_HEIGHT,0,GL_LIGHTMAP_FORMAT,GL_UNSIGNED_BYTE,dummy);

	#else

	qglTexImage2D(GL_TEXTURE_2D,0,gl_lms.internal_format,BLOCK_WIDTH,BLOCK_HEIGHT,0,GL_LIGHTMAP_FORMAT,GL_UNSIGNED_BYTE,dummy);

	#endif

} 
 
/* 
======================= 
GL_EndBuildingLightmaps 
======================= 
*/
 
void GL_EndBuildingLightmaps (void) 
{ 
	LM_UploadBlock( false ); 
	GL_EnableMultitexture( false ); 
} 
 
