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
#include "r_local.h"

vec3_t r_pright, r_pup, r_ppn;
extern cvar_t *sw_custom_particles;

#define PARTICLE_33     0
#define PARTICLE_66     1
#define PARTICLE_OPAQUE 2

/*

typedef struct
{
	particle_t *particle;
	int         level;
	int         color;

} partparms_t;

static partparms_t partparms;

static byte BlendParticle33( int pcolor, int dstcolor )
{
	return vid.alphamap[pcolor + dstcolor*256];
}

static byte BlendParticle66( int pcolor, int dstcolor )
{
	return vid.alphamap[pcolor*256+dstcolor];
}

static byte BlendParticle100( int pcolor, int dstcolor )
{
	dstcolor = dstcolor;
	return pcolor;
}
*/

/*
** R_DrawParticle
**
** Yes, this is amazingly slow, but it's the C reference
** implementation and should be both robust and vaguely
** understandable.  The only time this path should be
** executed is if we're debugging on x86 or if we're
** recompiling and deploying on a non-x86 platform.
**
** To minimize error and improve readability I went the 
** function pointer route.  This exacts some overhead, but
** it pays off in clean and easy to understand code.
*/

//static void R_DrawParticle( void )
static void R_DrawParticle( particle_t *pparticle, int level ) // new Cowcat
{
	//particle_t 	*pparticle = partparms.particle;
	//int		level = partparms.level;
	vec3_t		local, transformed;
	float		zi;
	byte		*pdest;
	short		*pz;
	int      	color = pparticle->color;
	int		i, izi, pix, count, u, v;
	//byte		(*blendparticle)( int, int );
	int		custom_particle = (int)sw_custom_particles->value;

	/*
	** transform the particle
	*/
	VectorSubtract (pparticle->origin, r_origin, local);

	transformed[0] = DotProduct(local, r_pright);
	transformed[1] = DotProduct(local, r_pup);
	transformed[2] = DotProduct(local, r_ppn);		

	if (transformed[2] < PARTICLE_Z_CLIP)
		return;

	#if 0 // wtf - Cowcat
	/*
	** bind the blend function pointer to the appropriate blender
	*/
	if ( level == PARTICLE_33 )
		blendparticle = BlendParticle33;

	else if ( level == PARTICLE_66 )
		blendparticle = BlendParticle66;

	else 
		blendparticle = BlendParticle100;
	#endif

	/*
	** project the point
	*/
	// FIXME: preadjust xcenter and ycenter
	zi = 1.0 / transformed[2];
	u = (int)(xcenter + zi * transformed[0] + 0.5);
	v = (int)(ycenter - zi * transformed[1] + 0.5);

	if ((v > d_vrectbottom_particle) || (u > d_vrectright_particle) || (v < d_vrecty) || (u < d_vrectx))
	{
		return;
	}

	/*
	** compute addresses of zbuffer, framebuffer, and 
	** compute the Z-buffer reference value.
	*/
	pz = d_pzbuffer + (d_zwidth * v) + u;
	pdest = d_viewbuffer + d_scantable[v] + u;
	izi = (int)(zi * 0x8000);

	/*
	** determine the screen area covered by the particle,
	** which also means clamping to a min and max
	*/
	//pix = izi >> d_pix_shift;
	pix = (izi * d_pix_mul) >> 7; // 8 maybe ? new Cowcat

	if (pix < d_pix_min)
		pix = d_pix_min;

	else if (pix > d_pix_max)
		pix = d_pix_max;

	/*
	** render the appropriate pixels
	*/
	count = pix;

	//if ( (pz[(r_screenwidth * count / 2) + (count / 2)]) > izi)
		//return; // looks like under some object - new test Cowcat 

	if(custom_particle == 0)
	{
    		switch (level)
		{
    			case PARTICLE_33 :

        			for ( ; count ; count--, pz += d_zwidth, pdest += r_screenwidth)
        			{
					//FIXME--do it in blocks of 8?
            				for (i=0 ; i<pix ; i++)
            				{
                				if (pz[i] <= izi)
                				{
                    					pz[i]    = izi;
                    					pdest[i] = vid.alphamap[color + ((int)pdest[i]<<8)];
                				}
            				}
        			}

        			break;

    			case PARTICLE_66 :
			{
				int color_part = (color<<8); // new cowcat

        			for ( ; count ; count--, pz += d_zwidth, pdest += r_screenwidth)
        			{
            				for (i=0 ; i<pix ; i++)
            				{
                				if (pz[i] <= izi)
                				{
                    					pz[i]    = izi;
                    					//pdest[i] = vid.alphamap[(color<<8) + (int)pdest[i]];
							pdest[i] = vid.alphamap[color_part + (int)pdest[i]]; // new cowcat
						}
					}
				}

				break;
			}

			default:  //100

			for ( ; count ; count--, pz += d_zwidth, pdest += r_screenwidth)
			{
				for (i=0 ; i<pix ; i++)
            			{
                			if (pz[i] <= izi)
                			{
                    				pz[i]    = izi;
                    				pdest[i] = color;
                			}
            			}
        		}

        		break;
    		}
	}

	else
	{
		int min_int, max_int;
		min_int = pix / 2;
		max_int = (pix * 2) - min_int;

		switch (level)
		{
    			case PARTICLE_33 :

        		for ( ; count ; count--, pz += d_zwidth, pdest += r_screenwidth)
        		{
				//FIXME--do it in blocks of 8?
            			for (i=0 ; i<pix ; i++)
            			{
					int pos = i + count;

                			if (pos >= min_int && pos <= max_int && pz[i] <= izi)
                			{
                    				pz[i]    = izi;
                    				pdest[i] = vid.alphamap[color + ((int)pdest[i]<<8)];
                			}
            			}
        		}

        		break;

    			case PARTICLE_66 :
			{
				int color_part = (color<<8); // new cowcat

        			for ( ; count ; count--, pz += d_zwidth, pdest += r_screenwidth)
        			{
            				for (i=0 ; i<pix ; i++)
            				{
						int pos = i + count;

                				if (pos >= min_int && pos <= max_int && pz[i] <= izi)
                				{
                    					pz[i]    = izi;
                    					//pdest[i] = vid.alphamap[(color<<8) + (int)pdest[i]];
							pdest[i] = vid.alphamap[color_part + (int)pdest[i]]; // new cowcat
						}
					}
				}

				break;
			}

    			default:  //100

        		for ( ; count ; count--, pz += d_zwidth, pdest += r_screenwidth)
        		{
            			for (i=0 ; i<pix ; i++)
            			{
					int pos = i + count;

                			if (pos >= min_int && pos <= max_int && pz[i] <= izi)
                			{
                    				pz[i]    = izi;
                    				pdest[i] = color;
					}
				}
			}

        		break;
    		}
	}
}

/*
** R_DrawParticles
**
** Responsible for drawing all of the particles in the particle list
** throughout the world.  Doesn't care if we're using the C path or
** if we're using the asm path, it simply assigns a function pointer
** and goes.
*/

void R_DrawParticles (void)
{
	particle_t *p;
	int         i;

	VectorScale( vright, xscaleshrink, r_pright );
	VectorScale( vup, yscaleshrink, r_pup );
	VectorCopy( vpn, r_ppn );

	for (p=r_newrefdef.particles, i=0 ; i<r_newrefdef.num_particles ; i++,p++)
	{
		int level;

		if ( p->alpha > 0.66 )
			//partparms.level = PARTICLE_OPAQUE;
			level = PARTICLE_OPAQUE; 

		else if ( p->alpha > 0.33 )
			//partparms.level = PARTICLE_66;
			level = PARTICLE_66;


		else
			//partparms.level = PARTICLE_33;
			level = PARTICLE_33;

		/*
		partparms.particle = p;
		partparms.color    = p->color;

		R_DrawParticle();
		*/

		R_DrawParticle(p, level); // new Cowcat
	}
}
