//surgeon: added AMIGA_ARGB_4444 define for
//MGL_UNSIGNED_SHORT_4_4_4_4 lightmapformat

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
// r_light.c

#include "gl_local.h"

int	r_dlightframecount;

#define	DLIGHT_CUTOFF	64

/*
=============================================================================

DYNAMIC LIGHTS BLEND RENDERING

=============================================================================
*/

#if 0

#undef qglColor3f
#undef qglVertex3fv

#define qglColor3f(red,green,blue) {\
CC->CurrentColor.r = (W3D_Float)(red);\
CC->CurrentColor.g = (W3D_Float)(green);\
CC->CurrentColor.b = (W3D_Float)(blue);\
CC->CurrentColor.a = 1.f;\
CC->UpdateCurrentColor = GL_TRUE;\
}\

#define qglVertex3fv(v){\
	if(CC->ShadeModel == GL_SMOOTH)\
	{\
		MGLVERT.v.color.r = CC->CurrentColor.r;\
		MGLVERT.v.color.g = CC->CurrentColor.g;\
		MGLVERT.v.color.b = CC->CurrentColor.b;\
		MGLVERT.v.color.a = CC->CurrentColor.a;\
	}\
	MGLVERT.bx=((GLfloat*)(v))[0];\
	MGLVERT.by=((GLfloat*)(v))[1];\
	MGLVERT.bz=((GLfloat*)(v))[2];\
	MGLVERT.bw=(GLfloat)1.f;\
	CC->VertexBufferPointer++;\
}\

#endif

#ifdef AMIGAOS //surgeon

float glowcos[17] =  
{ 
	 1.000000f, 
	 0.923880f, 
	 0.707105f, 
	 0.382680f, 
	 0.000000f, 
	-0.382680f, 
	-0.707105f, 
	-0.923880f, 
	-1.000000f, 
	-0.923880f, 
	-0.707105f, 
	-0.382680f, 
	 0.000000f, 
	 0.382680f, 
	 0.707105f, 
	 0.923880f, 
	 1.000000f 
}; 
 
float glowsin[17] =  
{ 
	 0.000000f, 
	 0.382680f, 
	 0.707105f, 
	 0.923880f, 
	 1.000000f, 
	 0.923880f, 
	 0.707105f, 
	 0.382680f, 
	-0.000000f, 
	-0.382680f, 
	-0.707105f, 
	-0.923880f, 
	-1.000000f, 
	-0.923880f, 
	-0.707105f, 
	-0.382680f, 
	 0.000000f 
}; 

//surgeon
void V_AddBlend(float r, float g, float b, float intensity, float *blend)
{
	blend[0] += r * intensity;
	blend[1] += g * intensity;
	blend[2] += b * intensity;

	if(blend[0] > 1.f) blend[0] = 1.f;
	if(blend[1] > 1.f) blend[1] = 1.f;
	if(blend[2] > 1.f) blend[2] = 1.f;

	if(!blend[3])
		blend[3] = intensity;
}

void R_RenderDlight (dlight_t *light)
{
	int	i, j;
	vec3_t	v;
	float	rad;

	rad = light->intensity * 0.35;

	VectorSubtract (light->origin, r_origin, v);

#if 1
	if (VectorLength (v) < rad)
	{
		extern float v_blend[4]; //surgeon
		// view is inside the dlight

		V_AddBlend (light->color[0], light->color[1], light->color[2], 0.15, v_blend);
		return;
	}
#endif

	qglBegin (GL_TRIANGLE_FAN);

	qglColor3f (light->color[0]*0.2, light->color[1]*0.2, light->color[2]*0.2);

	for (i=0 ; i<3 ; i++)
		v[i] = light->origin[i] - vpn[i]*rad;

	qglVertex3fv (v);
	qglColor3f (0,0,0);

	for (i=16 ; i>=0 ; i--)
	{
		for (j=0 ; j<3 ; j++)
			v[j] = light->origin[j] + vright[j]*glowcos[i]*rad + vup[j]*glowsin[i]*rad;

		qglVertex3fv (v);
	}

	qglEnd ();
}

#else

void R_RenderDlight (dlight_t *light)
{
	int	i, j;
	float	a;
	vec3_t	v;
	float	rad;

	rad = light->intensity * 0.35;

	VectorSubtract (light->origin, r_origin, v);
#if 0
	// FIXME?
	if (VectorLength (v) < rad)
	{	// view is inside the dlight
		V_AddBlend (light->color[0], light->color[1], light->color[2], light->intensity * 0.0003, v_blend);
		return;
	}
#endif

	qglBegin (GL_TRIANGLE_FAN);
	qglColor3f (light->color[0]*0.2, light->color[1]*0.2, light->color[2]*0.2);

	for (i=0 ; i<3 ; i++)
		v[i] = light->origin[i] - vpn[i]*rad;

	qglVertex3fv (v);
	qglColor3f (0,0,0);

	for (i=16 ; i>=0 ; i--)
	{
		a = i/16.0 * M_PI*2;

		for (j=0 ; j<3 ; j++)
			v[j] = light->origin[j] + vright[j]*cos(a)*rad + vup[j]*sin(a)*rad;

		qglVertex3fv (v);
	}

	qglEnd ();
}

#endif

/*
=============
R_RenderDlights
=============
*/

extern qboolean fog; // Cowcat

void R_RenderDlights (void)
{
	int		i;
	dlight_t	*l;

	#ifndef AMIGAOS

	if (!gl_flashblend->value)
		return;

	r_dlightframecount = r_framecount + 1;	// because the count hasn't advanced yet for this frame

	#else

	if (gl_flashblend->value == 0)
		return;

	if(gl_flashblend->value == 1)
		r_dlightframecount = r_framecount + 1;
 
	#endif

	if(fog)
		qglDisable(GL_FOG); //

	qglDepthMask (GL_FALSE);
	qglDisable (GL_TEXTURE_2D);
	qglShadeModel (GL_SMOOTH);
	qglEnable (GL_BLEND);
	qglBlendFunc (GL_ONE, GL_ONE);

	l = r_newrefdef.dlights;

	for (i=0 ; i<r_newrefdef.num_dlights ; i++, l++)
		R_RenderDlight (l);

	qglColor3f (1,1,1);
	qglDisable (GL_BLEND);
	qglEnable (GL_TEXTURE_2D);
	qglBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	qglDepthMask (GL_TRUE);

	if(fog)
		qglEnable(GL_FOG); //

}


/*
=============================================================================

DYNAMIC LIGHTS

=============================================================================
*/

#if 0

/*
=============
R_MarkLights
=============
*/

void R_MarkLights (dlight_t *light, int bit, mnode_t *node)
{
	cplane_t	*splitplane;
	float		dist;
	msurface_t	*surf;
	int		i, sidebit;

loc0:	
	if (node->contents != -1)
		return;

	splitplane = node->plane;
	dist = DotProduct (light->origin, splitplane->normal) - splitplane->dist;
	
	if (dist > light->intensity-DLIGHT_CUTOFF)
	{
		//R_MarkLights (light, bit, node->children[0]);
		//return;
		node = node->children[0]; 
		goto loc0; 
	}

	if (dist < -light->intensity+DLIGHT_CUTOFF)
	{
		//R_MarkLights (light, bit, node->children[1]);
		//return;
		node = node->children[1]; 
		goto loc0;
	}
		
	// mark the polygons
	surf = r_worldmodel->surfaces + node->firstsurface;

	for (i=0 ; i<node->numsurfaces ; i++, surf++)
	{

		// KnightMare - Discoloda´s dynamic light clipping
		//if(r_dlights_normal->value)
		//{
			dist = DotProduct (light->origin, surf->plane->normal) - surf->plane->dist;

			if (dist >= 0)
				sidebit = 0;
			else
				sidebit = SURF_PLANEBACK;

			if ( (surf->flags & SURF_PLANEBACK) != sidebit )
				continue;
		//}

		if (surf->dlightframe != r_dlightframecount)
		{
			surf->dlightbits = bit; // was 0
			//memset (surf->dlightbits, 0, sizeof(surf->dlightbits));
			//surf->dlightbits[num >> 5] = 1 << (num & 31);
			surf->dlightframe = r_dlightframecount;
		}
		else
			surf->dlightbits |= bit;
			//surf->dlightbits[num >> 5] | = 1 << (num & 31);
	}

	R_MarkLights (light, bit, node->children[0]);
	R_MarkLights (light, bit, node->children[1]);
}

#else

//surgeon: optimization adapted from LordHavoc's darkplaces engine

/*
=============
R_MarkLights
=============
*/

void R_MarkLights (dlight_t *light, int bit, mnode_t *node)
{
	cplane_t	*splitplane;
	float		dist;
	msurface_t	*surf;
	int		i; 
	float		l, maxdist; 
	int		j, s, t; 
	vec3_t		impact; 

loc0: 
	if (node->contents != -1)
		return;

	splitplane = node->plane; 

	if (splitplane->type < 3)
		dist = light->origin[splitplane->type] - splitplane->dist;
 
	else
		dist = DotProduct (light->origin, splitplane->normal) - splitplane->dist; // LordHavoc: original code 
	
	if (dist > light->intensity-DLIGHT_CUTOFF)
	{ 
		node = node->children[0]; 
		goto loc0; 
	}

	if (dist < -light->intensity+DLIGHT_CUTOFF)
	{
		node = node->children[1]; 
		goto loc0; 
	}

	maxdist = (light->intensity-DLIGHT_CUTOFF)*(light->intensity-DLIGHT_CUTOFF);
 
	// mark the polygons
	surf = r_worldmodel->surfaces + node->firstsurface;

	for (i=0 ; i<node->numsurfaces ; i++, surf++)
	{ 
		// LordHavoc: MAJOR dynamic light speedup here, eliminates marking of surfaces that are too far away from light, 
		// thus preventing unnecessary renders and uploads
 
		for (j=0 ; j<3 ; j++) 
			impact[j] = light->origin[j] - surf->plane->normal[j]*dist; 
 
		// clamp center of light to corner and check brightness 
		l = DotProduct (impact, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3] - surf->texturemins[0];

		s = l + 0.5;

		if (s < 0)
			s = 0;

		else if (s > surf->extents[0])
			s = surf->extents[0]; 

		s = l - s; 

		l = DotProduct (impact, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3] - surf->texturemins[1];
 
		t = l + 0.5;

		if (t < 0)
			t = 0;

		else if (t > surf->extents[1])
			t = surf->extents[1];
 
		t = l - t;
 
		// compare to minimum light 

		if ( (s*s + t*t + dist*dist) < maxdist ) 
		{ 
			if (surf->dlightframe != r_dlightframecount) // not dynamic until now 
			{ 
				surf->dlightbits = bit; 
				surf->dlightframe = r_dlightframecount; 
			}
 
			else // already dynamic 
				surf->dlightbits |= bit; 
		} 
	}

	if (node->children[0]->contents == -1) 
	{ 
		if (node->children[1]->contents == -1) 
		{ 
			R_MarkLights (light, bit, node->children[0]); 
			node = node->children[1]; 
			goto loc0; 
		}

		else 
		{ 
			node = node->children[0]; 
			goto loc0; 
		} 
	}

	else if (node->children[1]->contents == -1) 
	{ 
		node = node->children[1]; 
		goto loc0; 
	} 

}

#endif

/*
=============
R_PushDlights
=============
*/

void R_PushDlights (void)
{
	int		i;
	dlight_t	*l;

	#ifndef AMIGAOS

	if (gl_flashblend->value)
		return;

	#else

	if (gl_flashblend->value == 1)
		return;

	#endif

	r_dlightframecount = r_framecount + 1;	// because the count hasn't advanced yet for this frame

	l = r_newrefdef.dlights;

	for (i=0 ; i<r_newrefdef.num_dlights ; i++, l++)
		R_MarkLights ( l, 1<<i, r_worldmodel->nodes );
}


/*
=============================================================================

LIGHT SAMPLING

=============================================================================
*/

vec3_t		pointcolor;
cplane_t	*lightplane;	// used as shadow plane (where ? - Cowcat)
vec3_t		lightspot;	// for shadows

#if 0

void VectorMA2 (vec3_t veca, float scale, unsigned char *b, vec3_t vecc)
{
	vecc[0] = veca[0] + scale*b[0];
	vecc[1] = veca[1] + scale*b[1];
	vecc[2] = veca[2] + scale*b[2];
}

static int LightPoint_RecursiveBSPNode (model_t *model, vec3_t ambientcolor, const mnode_t *node, float x, float y, float startz, float endz)
{
   	int side;
   	float front, back;
   	float mid, distz = endz - startz;

loc0:
   	//if (!node->plane)
   	if (node->contents != -1)
      		return false;      // didn't hit anything

   	switch (node->plane->type)
   	{
   		case PLANE_X:
      			node = node->children[x < node->plane->dist];
      			goto loc0;

   		case PLANE_Y:
      			node = node->children[y < node->plane->dist];
      			goto loc0;

   		case PLANE_Z:
      			side = startz < node->plane->dist;

      			if ((endz < node->plane->dist) == side)
      			{
         			node = node->children[side];
         			goto loc0;
      			}

      			// found an intersection
      			mid = node->plane->dist;
      			break;

   		default:
      			back = front = x * node->plane->normal[0] + y * node->plane->normal[1];
      			front += startz * node->plane->normal[2];
      			back += endz * node->plane->normal[2];
      			side = front < node->plane->dist;

      			if ((back < node->plane->dist) == side)
      			{
         			node = node->children[side];
         			goto loc0;
      			}

      			// found an intersection
      			mid = startz + distz * (front - node->plane->dist) / (front - back);
      			break;
   	}

   	// go down front side
   	if (node->children[side]->plane && LightPoint_RecursiveBSPNode(model, ambientcolor, node->children[side], x, y, startz, mid))
   	{
      		return true;   // hit something
   	}

   	else
   	{
      		// check for impact on this node
      		if (node->numsurfaces)
      		{
         		unsigned int i;
         		int dsi, dti, lmwidth, lmheight;
         		float ds, dt;
         		msurface_t *surface;
         		unsigned char *lightmap;
         		int maps, line3, size3;
         		float dsfrac;
         		float dtfrac;
         		float w00, w01, w10, w11;

         		surface = model->surfaces + node->firstsurface;

         		for (i = 0; i < node->numsurfaces; ++i, ++surface)
         		{
            			if (surface->flags & (SURF_DRAWTURB|SURF_DRAWSKY))
               				continue;   // no lightmaps

            // location we want to sample in the lightmap
            ds = ((x * surface->texinfo->vecs[0][0] + y * surface->texinfo->vecs[0][1] + mid * surface->texinfo->vecs[0][2] + surface->texinfo->vecs[0][3]) - surface->texturemins[0]) * 0.0625f;
            dt = ((x * surface->texinfo->vecs[1][0] + y * surface->texinfo->vecs[1][1] + mid * surface->texinfo->vecs[1][2] + surface->texinfo->vecs[1][3]) - surface->texturemins[1]) * 0.0625f;

            if (ds >= 0.0f && dt >= 0.0f) // jit - fix for negative light values
            {
               int dsi = (int)ds;
               int dti = (int)dt;

               lmwidth = ((surface->extents[0] >> 4) + 1);
               lmheight = ((surface->extents[1] >> 4) + 1);

               // is it in bounds?
               if (dsi < lmwidth && dti < lmheight) // jit - fix for black models right on brush splits.
               {

                  // calculate bilinear interpolation factors
                  // and also multiply by fixedpoint conversion factors
                  dsfrac = ds - dsi;
                  dtfrac = dt - dti;

                  w00 = (1 - dsfrac) * (1 - dtfrac) * (1.0f / 255.0f);
                  w01 = (    dsfrac) * (1 - dtfrac) * (1.0f / 255.0f);
                  w10 = (1 - dsfrac) * (    dtfrac) * (1.0f / 255.0f);
                  w11 = (    dsfrac) * (    dtfrac) * (1.0f / 255.0f);

                  // values for pointer math
                  line3 = lmwidth * 3;
                  size3 = lmwidth * lmheight * 3;

                  // look up the pixel
                  lightmap = surface->samples + dti * line3 + dsi * 3;
                  //lightmap = surface->stain_samples + dti * line3 + dsi * 3; // Note: comment this line out and use the one above if you do not have stainmaps

                  // bilinear filter each lightmap style, and sum them
                  for (maps = 0; maps < MAXLIGHTMAPS && surface->styles[maps] != 255; maps++)
                  {
                     VectorMA2(ambientcolor, w00, lightmap            , ambientcolor);
                     VectorMA2(ambientcolor, w01, lightmap + 3        , ambientcolor);
                     VectorMA2(ambientcolor, w10, lightmap + line3    , ambientcolor);
                     VectorMA2(ambientcolor, w11, lightmap + line3 + 3, ambientcolor);

                     lightmap += size3;
                  }

                  return true; // success
               }
            }
         }
      }

      // go down back side
      node = node->children[side ^ 1];
      startz = mid;
      distz = endz - startz;
      goto loc0;
   }
}

#endif

int RecursiveLightPoint (mnode_t *node, vec3_t start, vec3_t end)
{
	float		front, back, frac;
	int		side;
	cplane_t	*plane;
	vec3_t		mid;
	msurface_t	*surf;
	int		s, t, ds, dt;
	int		i;
	mtexinfo_t	*tex;
	byte		*lightmap;
	int		maps;
	int		r;

loc0: //surgeon: optimized recursion

	if (node->contents != -1)
		return -1;		// didn't hit anything
	
	// calculate mid point

	#if 0

	// FIXME: optimize for axial
	plane = node->plane;
	front = DotProduct (start, plane->normal) - plane->dist;
	back = DotProduct (end, plane->normal) - plane->dist;
	side = front < 0;

	#else

	#if 0

	//optimization borrowed from LordHavoc (darkplaces engine)

	if (node->plane->type < 3)
        {
		front = start[node->plane->type] - node->plane->dist;
                back = end[node->plane->type] - node->plane->dist;
        }

        else
        {
                front = DotProduct(start, node->plane->normal) - node->plane->dist;
                back = DotProduct(end, node->plane->normal) - node->plane->dist;
        }

	side = front < 0;

	#else

	plane = node->plane;  // Cowcat

	if (plane->type < 3)
        {
                 front = start[plane->type] - plane->dist;
                 back = end[plane->type] - plane->dist;
        }

        else
        {
        	front = DotProduct(start, plane->normal) - plane->dist;
        	back = DotProduct(end, plane->normal) - plane->dist;
        }

	side = front < 0;

	#endif

	#endif

	#if 0

	if ( (back < 0) == side)
		return RecursiveLightPoint (node->children[side], start, end);

	#else

	//optimization borrowed from LordHavoc (darkplaces engine)

	if ((back < 0) == side)
	{
		node = node->children[side];
		goto loc0;
	}

	#endif
	
	frac = front / (front-back);
	mid[0] = start[0] + (end[0] - start[0])*frac;
	mid[1] = start[1] + (end[1] - start[1])*frac;
	mid[2] = start[2] + (end[2] - start[2])*frac;
	
	// go down front side	
	r = RecursiveLightPoint (node->children[side], start, mid);

	if (r >= 0)
		return r;		// hit something
		
	if ( (back < 0) == side )
		return -1;		// didn't hit anuthing
		
	// check for impact on this node
	VectorCopy (mid, lightspot); 	// this is for shadows -Cowcat
	lightplane = plane;

	surf = r_worldmodel->surfaces + node->firstsurface;

	for (i=0 ; i<node->numsurfaces ; i++, surf++)
	{
		if (surf->flags&(SURF_DRAWTURB|SURF_DRAWSKY)) 
			continue;	// no lightmaps

		tex = surf->texinfo;
		
		s = DotProduct (mid, tex->vecs[0]) + tex->vecs[0][3];
		t = DotProduct (mid, tex->vecs[1]) + tex->vecs[1][3];

		if (s < surf->texturemins[0] || t < surf->texturemins[1])
			continue;
		
		ds = s - surf->texturemins[0];
		dt = t - surf->texturemins[1];
		
		if ( ds > surf->extents[0] || dt > surf->extents[1] )
			continue;

		if (!surf->samples)
			return 0;

		ds >>= 4;
		dt >>= 4;

		lightmap = surf->samples;
		VectorCopy (vec3_origin, pointcolor);

		if (lightmap)
		{
			vec3_t scale;

			lightmap += 3 * (dt * ((surf->extents[0]>>4)+1) + ds);

			for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ; maps++)
			{
				for (i=0 ; i<3 ; i++)
					scale[i] = gl_modulate->value*r_newrefdef.lightstyles[surf->styles[maps]].rgb[i];

				pointcolor[0] += lightmap[0] * scale[0] * (1.0/255);
				pointcolor[1] += lightmap[1] * scale[1] * (1.0/255);
				pointcolor[2] += lightmap[2] * scale[2] * (1.0/255);
				lightmap += 3*((surf->extents[0]>>4)+1) * ((surf->extents[1]>>4)+1);
			}
		}
		
		return 1;
	}

	// go down back side
	#if 0

	return RecursiveLightPoint (node->children[!side], mid, end);

	#else //surgeon: optimized recursion

	node = node->children[!side]; 
	start[2] = mid[2]; 
	goto loc0;

	#endif
}

/*
===============
R_LightPoint
===============
*/

void R_LightPoint (vec3_t p, vec3_t color)
{
	vec3_t		end;
	float		r;
	int		lnum;
	dlight_t	*dl;
	//float		light;
	vec3_t		dist;
	float		add;

	#if 1 //surgeon: for optimized recursion
	vec3_t		p_tmp;	
	#endif

	if (!r_worldmodel->lightdata)
	{
		color[0] = color[1] = color[2] = 1.0;
		return;
	}
	
	end[0] = p[0];
	end[1] = p[1];
	end[2] = p[2] - 2048;

	#if 1 //surgeon: for optimized recursion

	VectorCopy (p, p_tmp);
	r = RecursiveLightPoint (r_worldmodel->nodes, p_tmp, end);

	#else

	r = RecursiveLightPoint (r_worldmodel->nodes, p, end);

	#endif

	/*
	VectorClear(pointcolor);
   	r = LightPoint_RecursiveBSPNode(r_worldmodel, pointcolor, r_worldmodel->nodes, p[0], p[1], p[2], p[2] - 2048.0f);

   	if (r <= 0)
	*/

	if (r == -1)
	{
		VectorCopy (vec3_origin, color);
	}

	else
	{
		VectorCopy (pointcolor, color);
	}

	//
	// add dynamic lights
	//

	//light = 0;
	dl = r_newrefdef.dlights;

	for (lnum=0 ; lnum<r_newrefdef.num_dlights ; lnum++, dl++)
	{
		VectorSubtract (currententity->origin, dl->origin, dist);
		add = dl->intensity - VectorLength(dist);
		add *= (1.0/256);

		if (add > 0)
		{
			VectorMA (color, add, dl->color, color);
		}
	}

	VectorScale (color, gl_modulate->value, color);
}


//===================================================================

static float s_blocklights[34*34*3];

/*
===============
R_AddDynamicLights
===============
*/

void R_AddDynamicLights (msurface_t *surf)
{
	int		lnum;
	int		sd, td;
	float		fdist, frad, fminlight;
	vec3_t		impact, local;
	int		s, t;
	int		i;
	int		smax, tmax;
	mtexinfo_t	*tex;
	dlight_t	*dl;
	float		*pfBL;
	float		fsacc, ftacc;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	tex = surf->texinfo;

	for (lnum=0 ; lnum<r_newrefdef.num_dlights ; lnum++)
	{
		if ( !(surf->dlightbits & (1<<lnum) ) )
			continue;		// not lit by this light

		dl = &r_newrefdef.dlights[lnum];
		frad = dl->intensity;
		fdist = DotProduct (dl->origin, surf->plane->normal) - surf->plane->dist;
		frad -= fabs(fdist);
		// rad is now the highest intensity on the plane

		fminlight = DLIGHT_CUTOFF;	// FIXME: make configurable?

		if (frad < fminlight)
			continue;

		fminlight = frad - fminlight;

		for (i=0 ; i<3 ; i++)
		{
			impact[i] = dl->origin[i] - surf->plane->normal[i]*fdist;
		}

		local[0] = DotProduct (impact, tex->vecs[0]) + tex->vecs[0][3] - surf->texturemins[0];
		local[1] = DotProduct (impact, tex->vecs[1]) + tex->vecs[1][3] - surf->texturemins[1];

		pfBL = s_blocklights;

		for (t = 0, ftacc = 0 ; t<tmax ; t++, ftacc += 16)
		{
			td = local[1] - ftacc;

			if ( td < 0 )
				td = -td;

			for ( s=0, fsacc = 0 ; s<smax ; s++, fsacc += 16, pfBL += 3)
			{
				sd = Q_ftol( local[0] - fsacc );

				if ( sd < 0 )
					sd = -sd;

				if (sd > td)
					fdist = sd + (td>>1);

				else
					fdist = td + (sd>>1);

				if ( fdist < fminlight )
				{
					pfBL[0] += ( frad - fdist ) * dl->color[0];
					pfBL[1] += ( frad - fdist ) * dl->color[1];
					pfBL[2] += ( frad - fdist ) * dl->color[2];
				}
			}
		}
	}
}

/*
** R_SetCacheState
*/

void R_SetCacheState( msurface_t *surf )
{
	int maps;

	for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ; maps++)
	{
		surf->cached_light[maps] = r_newrefdef.lightstyles[surf->styles[maps]].white;
	}
}

/*
===============
R_BuildLightMap

Combine and scale multiple lightmaps into the floating format in blocklights
===============
*/

void R_BuildLightMap (msurface_t *surf, byte *dest, int stride)
{
	int		smax, tmax;
	int		r, g, b, a, max;
	int		i, j, size;
	byte		*lightmap;
	float		scale[4];
	int		nummaps;
	float		*bl;
	//lightstyle_t	*style;
	int 		monolightmap;

	if ( surf->texinfo->flags & (SURF_SKY|SURF_TRANS33|SURF_TRANS66|SURF_WARP) )
		ri.Sys_Error (ERR_DROP, "R_BuildLightMap called for non-lit surface");

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	size = smax*tmax;

	if (size > (sizeof(s_blocklights)>>4) )
		ri.Sys_Error (ERR_DROP, "Bad s_blocklights size");

	// set to full bright if no light data
	if (!surf->samples)
	{
		//int maps;

		for (i=0 ; i<size*3 ; i++)
			s_blocklights[i] = 255;

		/* // that never works - Cowcat
		for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ; maps++)
		{
			style = &r_newrefdef.lightstyles[surf->styles[maps]];
		}
		*/

		goto store;
	}

	// count the # of maps
	#if 0

	for ( nummaps = 0 ; nummaps < MAXLIGHTMAPS && surf->styles[nummaps] != 255 ; nummaps++)
		;

	#else

	//surgeon: this code looks better

	nummaps = 0;

	while (nummaps < MAXLIGHTMAPS && surf->styles[nummaps] != 255)
	{
		nummaps++;
	}

	#endif

	lightmap = surf->samples;

	// add all the lightmaps
	if ( nummaps == 1 )
	{
		int maps;

		for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ; maps++)
		{
			bl = s_blocklights;

			for (i=0 ; i<3 ; i++)
				scale[i] = gl_modulate->value * r_newrefdef.lightstyles[surf->styles[maps]].rgb[i];

			if ( scale[0] == 1.0F && scale[1] == 1.0F && scale[2] == 1.0F )
			{
				for (i=0 ; i<size ; i++, bl+=3)
				{
					bl[0] = lightmap[i*3+0];
					bl[1] = lightmap[i*3+1];
					bl[2] = lightmap[i*3+2];
				}
			}

			else
			{
				for (i=0 ; i<size ; i++, bl+=3)
				{
					bl[0] = lightmap[i*3+0] * scale[0];
					bl[1] = lightmap[i*3+1] * scale[1];
					bl[2] = lightmap[i*3+2] * scale[2];
				}
			}

			lightmap += size*3;	// skip to next lightmap
		}
	}

	else
	{
		int maps;

		memset( s_blocklights, 0, sizeof( s_blocklights[0] ) * size * 3 );

		for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ; maps++)
		{
			bl = s_blocklights;

			for (i=0 ; i<3 ; i++)
				scale[i] = gl_modulate->value * r_newrefdef.lightstyles[surf->styles[maps]].rgb[i];

			if ( scale[0] == 1.0F && scale[1] == 1.0F && scale[2] == 1.0F )
			{
				for (i=0 ; i<size ; i++, bl+=3 )
				{
					bl[0] += lightmap[i*3+0];
					bl[1] += lightmap[i*3+1];
					bl[2] += lightmap[i*3+2];
				}
			}

			else
			{
				for (i=0 ; i<size ; i++, bl+=3)
				{
					bl[0] += lightmap[i*3+0] * scale[0];
					bl[1] += lightmap[i*3+1] * scale[1];
					bl[2] += lightmap[i*3+2] * scale[2];
				}
			}

			lightmap += size*3;	// skip to next lightmap
		}
	}

	// add all the dynamic lights
	if (surf->dlightframe == r_framecount)
		R_AddDynamicLights (surf);

	// put into texture format
store:

	#ifdef AMIGA_ARGB_4444 //surgeon
	stride -= (smax<<1);
	#else
	stride -= (smax<<2);
	#endif

	bl = s_blocklights;

	monolightmap = gl_monolightmap->string[0];

	if ( monolightmap == '0' )
	{
		for (i=0 ; i<tmax ; i++, dest += stride)
		{
			for (j=0 ; j<smax ; j++)
			{			
				r = Q_ftol( bl[0] );
				g = Q_ftol( bl[1] );
				b = Q_ftol( bl[2] );

				// catch negative lights
				if (r < 0)
					r = 0;

				if (g < 0)
					g = 0;

				if (b < 0)
					b = 0;

				/*
				** determine the brightest of the three color components
				*/
				if (r > g)
					max = r;

				else
					max = g;

				if (b > max)
					max = b;

				/*
				** alpha is ONLY used for the mono lightmap case.  For this reason
				** we set it to the brightest of the color components so that 
				** things don't get too dim.
				*/
				a = max;

				/*
				** rescale all the color components if the intensity of the greatest
				** channel exceeds 1.0
				*/
				if (max > 255)
				{
					float t = 255.0F / max;

					r = r*t;
					g = g*t;
					b = b*t;
					a = a*t;
				}

				#ifdef AMIGA_ARGB_4444 //surgeon

				dest[0] = (a & 0xF0) | ((r & 0xF0) >> 4);
				dest[1] = (g & 0xF0) | ((b & 0xF0) >> 4);

				bl += 3;
				dest += 2;

				#else

				dest[0] = r;
				dest[1] = g;
				dest[2] = b;
				dest[3] = a;

				bl += 3;
				dest += 4;

				#endif
			}
		}
	}

	else
	{
		for (i=0 ; i<tmax ; i++, dest += stride)
		{
			for (j=0 ; j<smax ; j++)
			{				
				r = Q_ftol( bl[0] );
				g = Q_ftol( bl[1] );
				b = Q_ftol( bl[2] );

				// catch negative lights
				if (r < 0)
					r = 0;

				if (g < 0)
					g = 0;

				if (b < 0)
					b = 0;

				/*
				** determine the brightest of the three color components
				*/
				if (r > g)
					max = r;

				else
					max = g;

				if (b > max)
					max = b;

				/*
				** alpha is ONLY used for the mono lightmap case.  For this reason
				** we set it to the brightest of the color components so that 
				** things don't get too dim.
				*/
				a = max;

				/*
				** rescale all the color components if the intensity of the greatest
				** channel exceeds 1.0
				*/
				if (max > 255)
				{
					float t = 255.0F / max;

					r = r*t;
					g = g*t;
					b = b*t;
					a = a*t;
				}

				/*
				** So if we are doing alpha lightmaps we need to set the R, G, and B
				** components to 0 and we need to set alpha to 1-alpha.
				*/

				switch ( monolightmap )
				{
					case 'L':
					case 'I':

					#ifndef AMIGAOS //surgeon: don't mess up vertexlighting

						r = a;
						g = b = 0;

					#else
						r=g=b = a;

					#endif
						break;

					case 'C':
						// try faking colored lighting
						a = 255 - ((r+g+b)/3);
						r *= a/255.0;
						g *= a/255.0;
						b *= a/255.0;
						break;

					case 'A':
					default:

					#ifndef AMIGAOS //surgeon: don't mess up vertexlighting

						r = g = b = 0;
					#endif
						a = 255 - a;
						break;
				}

				#ifdef AMIGA_ARGB_4444 //surgeon

				dest[0] = (a & 0xF0) | ((r & 0xF0) >> 4);
				dest[1] = (g & 0xF0) | ((b & 0xF0) >> 4);

				bl += 3;
				dest += 2;

				#else

				dest[0] = r;
				dest[1] = g;
				dest[2] = b;
				dest[3] = a;

				bl += 3;
				dest += 4;

				#endif
			}
		}
	}
}

