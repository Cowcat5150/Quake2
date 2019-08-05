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

#include "/client/client.h"
//#include <sys/time.h>

#pragma amiga-align
#include <proto/exec.h>
#pragma default-align

FILE            *cinematic_file;
int             cinematictime;
int             cinematicframe;
char            cinematicpalette[768];
qboolean        cinematicpalette_active;
extern 		unsigned char currentpalette[1024];
extern 		int s_rawend;
extern 		short ShortSwap(short l);
extern 		void SWimp_SetPalette (const unsigned char *palette);

typedef struct
{
 	int   		channels;
 	int   		samples;    	// mono samples in buffer
 	int   		submission_chunk;  // don't mix less than this #
 	int   		samplepos;    	// in mono samples
 	int   		samplebits;
 	int   		speed;
 	unsigned char  	*buffer;
} dma_t;

extern dma_t dma;

typedef struct
{
 	int   left;
 	int   right;

} portable_samplepair_t;

#define MAX_RAW_SAMPLES 8192

extern int paintedtime;
extern portable_samplepair_t   s_rawsamples[MAX_RAW_SAMPLES];
      
int vid_width=320;
int vid_height=240;
int vid_buffer[320*240];
int vid_rowbytes=320;
char *vid_buffer2;
long realtime=0;
float vid_gamma;
int gammatable[256];

typedef struct
{
	byte    *data;
	int	count;

} cblock_t;

typedef struct
{
	qboolean	restart_sound;
	int             s_rate;
	int             s_width;
	int             s_channels;

	int             width;
	int             height;
	byte    	*pic;
	byte    	*pic_pending;

	// order 1 huffman stuff
	int             *hnodes1;       // [256][256][2];
	int             numhnodes1[256];

	int             h_used[512];
	int             h_count[512];

} cinematics_t;

cinematics_t    cin;

connstate_t state;

int FS_filelength (FILE *f)
{
	int	pos;
	int	end;

	pos = ftell (f);
	fseek (f, 0, SEEK_END);
	end = ftell (f);
	fseek (f, pos, SEEK_SET);

	return end;
}

void FS_Read(void *buffer, int len, FILE *f)
{
    	fread (buffer, 1, len, f);
}


int FS_LoadFile (char *path, void **buffer)
{
	FILE    *h;
	byte    *buf;
	int     len;

	buf = NULL;     // quiet compiler warning

	h = fopen(path, "r");

	if (!h)
	{
		if (buffer)
			*buffer = NULL;

		return -1;
	}

	len = FS_filelength(h);

	if (!buffer)
	{
		fclose (h);
		return len;
	}

	buf = malloc(len);
	*buffer = buf;

	FS_Read (buf, len, h);

	fclose (h);

	return len;
}


inline int LittleLong(int l)
{
    union {
	unsigned char b[4];
	long f;
    } i,o;

    i.f = l;
    o.b[0] = i.b[3];
    o.b[1] = i.b[2];
    o.b[2] = i.b[1];
    o.b[3] = i.b[0];
    
    return o.f;
}

extern unsigned int timeGetTime();

int curtime;

int Sys_Milliseconds (void)
{
	curtime = timeGetTime();

	return curtime;
}

/*
==================
SmallestNode1
==================
*/

int SmallestNode1 (int numhnodes)
{
	int	i;
	int    	best, bestnode;

	best = 99999999;
	bestnode = -1;

	for (i=0 ; i<numhnodes ; i++)
	{
		if (cin.h_used[i])
			continue;

		if (!cin.h_count[i])
			continue;

		if (cin.h_count[i] < best)
		{
			best = cin.h_count[i];
			bestnode = i;
		}
	}

	if (bestnode == -1)
		return -1;

	cin.h_used[bestnode] = true;
	return bestnode;
}


/*
==================
Huff1TableInit

Reads the 64k counts table and initializes the node trees
==================
*/

void Huff1TableInit (void)
{
	int             prev;
	int             j;
	int             *node, *nodebase;
	byte    	counts[256];
	int             numhnodes;

	cin.hnodes1 = malloc (256*256*2*4);
	memset (cin.hnodes1, 0, 256*256*2*4);

	for (prev=0 ; prev<256 ; prev++)
	{
		memset (cin.h_count,0,sizeof(cin.h_count));
		memset (cin.h_used,0,sizeof(cin.h_used));

		// read a row of counts
		fread (counts, 1, sizeof(counts), cinematic_file);

		for (j=0 ; j<256 ; j++)
			cin.h_count[j] = counts[j];

		// build the nodes
		numhnodes = 256;
		nodebase = cin.hnodes1 + prev*256*2;

		while (numhnodes != 511)
		{
			node = nodebase + (numhnodes-256)*2;

			// pick two lowest counts
			node[0] = SmallestNode1 (numhnodes);

			if (node[0] == -1)
				break;  // no more

			node[1] = SmallestNode1 (numhnodes);

			if (node[1] == -1)
				break;

			cin.h_count[numhnodes] = cin.h_count[node[0]] + cin.h_count[node[1]];
			numhnodes++;
		}

		cin.numhnodes1[prev] = numhnodes-1;
	}
}

char *outbuffer;

/*
==================
Huff1Decompress
==================
*/
cblock_t Huff1Decompress (cblock_t in)
{
	byte            *input;
	byte            *out_p;
	int             nodenum;
	int             count;
	cblock_t        out;
	int             inbyte;
	int             *hnodes, *hnodesbase;
//int           i;

	// get decompressed count
	count = in.data[0] + (in.data[1]<<8) + (in.data[2]<<16) + (in.data[3]<<24);
	input = in.data + 4;
	out_p = out.data = malloc (count);
	outbuffer = out.data;

	// read bits

	hnodesbase = cin.hnodes1 - 256*2;       // nodes 0-255 aren't stored

	hnodes = hnodesbase;
	nodenum = cin.numhnodes1[0];
	while (count)
	{
		inbyte = *input++;
		//-----------
		if (nodenum < 256)
		{
			hnodes = hnodesbase + (nodenum<<9);
			*out_p++ = nodenum;
			if (!--count)
				break;
			nodenum = cin.numhnodes1[nodenum];
		}
		nodenum = hnodes[nodenum*2 + (inbyte&1)];
		inbyte >>=1;
		//-----------
		if (nodenum < 256)
		{
			hnodes = hnodesbase + (nodenum<<9);
			*out_p++ = nodenum;
			if (!--count)
				break;
			nodenum = cin.numhnodes1[nodenum];
		}
		nodenum = hnodes[nodenum*2 + (inbyte&1)];
		inbyte >>=1;
		//-----------
		if (nodenum < 256)
		{
			hnodes = hnodesbase + (nodenum<<9);
			*out_p++ = nodenum;
			if (!--count)
				break;
			nodenum = cin.numhnodes1[nodenum];
		}
		nodenum = hnodes[nodenum*2 + (inbyte&1)];
		inbyte >>=1;
		//-----------
		if (nodenum < 256)
		{
			hnodes = hnodesbase + (nodenum<<9);
			*out_p++ = nodenum;
			if (!--count)
				break;
			nodenum = cin.numhnodes1[nodenum];
		}
		nodenum = hnodes[nodenum*2 + (inbyte&1)];
		inbyte >>=1;
		//-----------
		if (nodenum < 256)
		{
			hnodes = hnodesbase + (nodenum<<9);
			*out_p++ = nodenum;
			if (!--count)
				break;
			nodenum = cin.numhnodes1[nodenum];
		}
		nodenum = hnodes[nodenum*2 + (inbyte&1)];
		inbyte >>=1;
		//-----------
		if (nodenum < 256)
		{
			hnodes = hnodesbase + (nodenum<<9);
			*out_p++ = nodenum;
			if (!--count)
				break;
			nodenum = cin.numhnodes1[nodenum];
		}
		nodenum = hnodes[nodenum*2 + (inbyte&1)];
		inbyte >>=1;
		//-----------
		if (nodenum < 256)
		{
			hnodes = hnodesbase + (nodenum<<9);
			*out_p++ = nodenum;
			if (!--count)
				break;
			nodenum = cin.numhnodes1[nodenum];
		}
		nodenum = hnodes[nodenum*2 + (inbyte&1)];
		inbyte >>=1;
		//-----------
		if (nodenum < 256)
		{
			hnodes = hnodesbase + (nodenum<<9);
			*out_p++ = nodenum;
			if (!--count)
				break;
			nodenum = cin.numhnodes1[nodenum];
		}
		nodenum = hnodes[nodenum*2 + (inbyte&1)];
		inbyte >>=1;
	}

	if (input - in.data != in.count && input - in.data != in.count+1)
	{

	}
	out.count = out_p - out.data;

	return out;
}


void SCR_LoadPCX (char *filename, byte **pic, byte **palette, int *width, int *height)
{
	byte    *raw;
	pcx_t   *pcx;
	int     x, y;
	int     len;
	int     dataByte, runLength;
	byte    *out, *pix;

	*pic = NULL;

	//
	// load the file
	//
	len = FS_LoadFile (filename, (void **)&raw);

	if (!raw)
		return; // Com_Printf ("Bad pcx file %s\n", filename);

	//
	// parse the PCX file
	//
	pcx = (pcx_t *)raw;

	raw = &pcx->data;

	if (pcx->manufacturer != 0x0a
		|| pcx->version != 5
		|| pcx->encoding != 1
		|| pcx->bits_per_pixel != 8
		|| pcx->xmax >= 640
		|| pcx->ymax >= 480)
	{
//		Com_Printf ("Bad pcx file %s\n", filename);
		return;
	}

	out = malloc ( (pcx->ymax+1) * (pcx->xmax+1) );

	*pic = out;

	pix = out;

	if (palette)
	{
		*palette = malloc(768);
		memcpy (*palette, (byte *)pcx + len - 768, 768);
	}

	if (width)
		*width = pcx->xmax+1;

	if (height)
		*height = pcx->ymax+1;

	for (y=0 ; y<=pcx->ymax ; y++, pix += pcx->xmax+1)
	{
		for (x=0 ; x<=pcx->xmax ; )
		{
			dataByte = *raw++;

			if((dataByte & 0xC0) == 0xC0)
			{
				runLength = dataByte & 0x3F;
				dataByte = *raw++;
			}

			else
				runLength = 1;

			while(runLength-- > 0)
				pix[x++] = dataByte;
		}

	}

	if ( raw - (byte *)pcx > len)
	{
		free (*pic);
		*pic = NULL;
	}

}

BOOL OpenDisplay(ULONG Width, ULONG Height);

int skipit=0;

void S_RawSamples (int samples, int rate, int width, int channels, byte *data)
{
	int     i;
	int     src, dst;
	float   scale;

	if (s_rawend < paintedtime)
		s_rawend = paintedtime;

	scale = (float)rate / dma.speed;

	if (channels == 2 && width == 2)
	{
		if (scale == 1.0)
		{
			for (i=0 ; i<samples ; i++)
			{
				dst = s_rawend&(MAX_RAW_SAMPLES-1);
				s_rawend++;
				s_rawsamples[dst].left = ShortSwap(((short *)data)[i*2]) << 8;
				s_rawsamples[dst].right = ShortSwap(((short *)data)[i*2+1]) << 8;
			}
		}

		else
		{
			for (i=0 ; ; i++)
			{
				src = i*scale;
				if (src >= samples)
					break;
				dst = s_rawend&(MAX_RAW_SAMPLES-1);
				s_rawend++;
				s_rawsamples[dst].left =
				    ShortSwap(((short *)data)[src*2]) << 8;
				s_rawsamples[dst].right =
				    ShortSwap(((short *)data)[src*2+1]) << 8;
			}
		}
	}

	else if (channels == 1 && width == 2)
	{
		for (i=0 ; ; i++)
		{
			src = i*scale;
			if (src >= samples)
				break;
			dst = s_rawend&(MAX_RAW_SAMPLES-1);
			s_rawend++;
			s_rawsamples[dst].left =
			    ShortSwap(((short *)data)[src]) << 8;
			s_rawsamples[dst].right =
			    ShortSwap(((short *)data)[src]) << 8;
		}
	}

	else if (channels == 2 && width == 1)
	{
		for (i=0 ; ; i++)
		{
			src = i*scale;
			if (src >= samples)
				break;
			dst = s_rawend&(MAX_RAW_SAMPLES-1);
			s_rawend++;
			s_rawsamples[dst].left =
			    ((char *)data)[src*2] << 16;
			s_rawsamples[dst].right =
			    ((char *)data)[src*2+1] << 16;
		}
	}

	else if (channels == 1 && width == 1)
	{
		for (i=0 ; ; i++)
		{
			src = i*scale;
			if (src >= samples)
				break;
			dst = s_rawend&(MAX_RAW_SAMPLES-1);
			s_rawend++;
			s_rawsamples[dst].left =
			    (((byte *)data)[src]-128) << 16;
			s_rawsamples[dst].right = (((byte *)data)[src]-128) << 16;
		}
	}
}


byte *SCR_ReadNextFrame (void)
{
 	int  	r;
 	int  	command;
 	byte 	samples[22050/14*4];
 	byte 	compressed[0x20000];
 	int  	size;
 	byte 	*pic;
 	cblock_t in, huf1;
 	int  	start, end, count;
 	long 	frame;

 	frame = -(realtime - cinematictime)*14.0/1000;
// printf("%i %i\n",frame,cinematicframe);

 if (frame <= cinematicframe)
 {
  	skipit=1;
  	return 1;
 }
 if (frame > cinematicframe+1)
 {
 		//printf ("Dropped frame: %i > %i\n", frame, cinematicframe+1);
 		cinematictime = -(realtime - cinematicframe*1000/14);
 }

 	// read the next frame
 	r = fread (&command, 4, 1, cinematic_file);
 	if (r == 0)  		// we'll give it one more chance
  		r = fread (&command, 4, 1, cinematic_file);

 	if (r != 1)
  		return NULL;

 	command = LittleLong(command);

 	if (command == 2)
  		return NULL; // last frame marker

 	if (command == 1)
 	{ 	// read palette
  		FS_Read (cinematicpalette, sizeof(cinematicpalette), cinematic_file);
  		cinematicpalette_active=0; 	// dubious....  exposes an edge case
 	}

 	// decompress the next frame
 	FS_Read (&size, 4, cinematic_file);
 	size = LittleLong(size);
 	if (size > sizeof(compressed) || size < 1)
  		return 0;
 	FS_Read (compressed, size, cinematic_file);

 	// read sound
 	start = cinematicframe*cin.s_rate/14;
 	end = (cinematicframe+1)*cin.s_rate/14;
 	count = end - start;

 	FS_Read (samples, count*cin.s_width*cin.s_channels, cinematic_file);

 	S_RawSamples (count, cin.s_rate, cin.s_width, cin.s_channels, samples);

 	in.data = compressed;
 	in.count = size;

 	huf1 = Huff1Decompress (in);

 	pic = huf1.data;

 	cinematicframe++;

	// printf("Frame: %i\n",cinematicframe);

 	return pic;
}

void Draw_BuildGammaTable (void)
{
 int     i, inf;
 float   g;

 g = vid_gamma;

 if (g == 1.0)
 {
  for (i=0 ; i<256 ; i++)
   gammatable[i] = i;
  return;
 }

 for (i=0 ; i<256 ; i++)
 {
  inf = 255 * pow ( (i+0.5)/255.5 , g ) + 0.5;
  if (inf < 0)
   inf = 0;
  if (inf > 255)
   inf = 255;
  gammatable[i] = inf;
 }
}

void DrawStretchRaw(int x, int y, int w, int h, int cin_w, int cin_h, byte *cin_data)  // was void *cin_data
{
 byte   *dest, *source;
 int    v, u, sv;
 int    height;
 int    f, fstep;
 int    skip;

 if ((x < 0) ||
  (x + w > vid_width) ||
  (y + h > vid_height))
 {
//  ri.Sys_Error (ERR_FATAL,"Draw_Pic: bad coordinates");
 }

 height = h;
 if (y < 0)
 {
  skip = -y;
  height += y;
  y = 0;
 }
 else
  skip = 0;

 dest = vid_buffer + y * vid_rowbytes + x;

 for (v=0 ; v<height ; v++, dest += vid_rowbytes)
 {
  sv = (skip + v)*cin_h/h;
  source = cin_data + sv*cin_w;
  if (w == cin_w)
   memcpy (dest, source, w);
  else
  {
   f = 0;
   fstep = cin_w*0x10000/w;
   for (u=0 ; u<w ; u+=4)
   {
    dest[u] = source[f>>16];
    f += fstep;
    dest[u+1] = source[f>>16];
    f += fstep;
    dest[u+2] = source[f>>16];
    f += fstep;
    dest[u+3] = source[f>>16];
    f += fstep;
   }
  }
 }
}

void SWimp_EndFrame(void);

void R_GammaCorrectAndSetPalette( const unsigned char *palette )
{
 int i;

 for ( i = 0; i < 256; i++ )
 {
  currentpalette[i*4+0] = gammatable[palette[i*4+0]];
  currentpalette[i*4+1] = gammatable[palette[i*4+1]];
  currentpalette[i*4+2] = gammatable[palette[i*4+2]];
 }
 currentpalette[0]=0;
 currentpalette[1]=0;
 currentpalette[2]=0;
 SWimp_SetPalette( currentpalette );
}

void CinematicSetPalette( const unsigned char *palette )
{
 byte palette32[1024];
 int  i, j, w;
 int  *d;

 // clear screen to black to avoid any palette flash

#if 0
 w = abs(vid_rowbytes)>>2;       // stupid negative pitch win32 stuff...
 for (i=0 ; i<vid_height ; i++, d+=w)
 {
  d = (int *)(vid_buffer + i*vid_rowbytes);
  for (j=0 ; j<w ; j++)
   d[j] = 0;
 }
#endif

 memset(vid_buffer,0,320*240);
 // flush it to the screen
 vid_buffer2=vid_buffer;
// SWimp_EndFrame ();

 if ( palette )
 {
  for ( i = 0; i < 256; i++ )
  {
   palette32[i*4+0] = palette[i*3+0];
   palette32[i*4+1] = palette[i*3+1];
   palette32[i*4+2] = palette[i*3+2];
   palette32[i*4+3] = 0xFF;
  }

	R_GammaCorrectAndSetPalette( palette32 );
 }
 else
 {
//  	printf("Wrong!\n");
//test	R_GammaCorrectAndSetPalette( ( const unsigned char * ) d_8to24table );
 }
}

void S_Update_();
void MainLoop();

/*
==================
SCR_PlayCinematic

==================
*/
void SCR_PlayCinematic (char *arg)
{
	int     width, height;
	byte    *palette;
	char    name[MAX_OSPATH], *dot;
	int     old_khz;

	cinematicframe = 0;
	Draw_BuildGammaTable();

	dot = strstr (arg, ".");
	if (dot && !strcmp (dot, ".pcx"))
	{
		sprintf (name, "%s", arg);
		SCR_LoadPCX (name, &cin.pic, &palette, &cin.width, &cin.height);
		cinematicframe = -1;
		cinematictime = 1;
		state = ca_active;
		if (!cin.pic)
		{
			cinematictime = 0;
		}
		else
		{
			memcpy (cinematicpalette, palette, sizeof(cinematicpalette));
			free (palette);

	  	DrawStretchRaw (0, 0, 320, 240, cin.width, cin.height, cin.pic);
			if (!cinematicpalette_active)
			{
				CinematicSetPalette(cinematicpalette);
	 			cinematicpalette_active = true;
			}
	    	vid_buffer2=vid_buffer;
	    	SWimp_EndFrame();
      		S_Update_();

		}
		return 1;
	}

	sprintf (name, "%s", arg);
	cinematic_file = fopen(name, "r");

	if (!cinematic_file)
	{
//FIXME:                SCR_FinishCinematic();
		cinematictime = 0;
		return 0;
	}


	state = ca_active;

	FS_Read (&width, 4, cinematic_file);
	FS_Read (&height, 4, cinematic_file);
	cin.width = LittleLong(width);
	cin.height = LittleLong(height);

	FS_Read (&cin.s_rate, 4, cinematic_file);
	cin.s_rate = LittleLong(cin.s_rate);
	FS_Read (&cin.s_width, 4, cinematic_file);
	cin.s_width = LittleLong(cin.s_width);
	FS_Read (&cin.s_channels, 4, cinematic_file);
	cin.s_channels = LittleLong(cin.s_channels);

	Huff1TableInit ();

	if (OpenDisplay(cin.width, cin.height) == FALSE)
	    return 0;

	// switch up to 22 khz sound if necessary
//FIXME:        old_khz = Cvar_VariableValue ("s_khz");
//        if (old_khz != cin.s_rate/1000)
//        {
//                cin.restart_sound = true;
//                Cvar_SetValue ("s_khz", cin.s_rate/1000);
//                CL_Snd_Restart_f ();
//                Cvar_SetValue ("s_khz", old_khz);
//        }

	cinematicframe = 0;
  	realtime=Sys_Milliseconds();
  	cinematictime = Sys_Milliseconds();
  	skipit=0;
  	outbuffer=0;

  while (cin.pic = SCR_ReadNextFrame())
  {
    cinematictime = Sys_Milliseconds();
    	if (!skipit)
    	{
	  	DrawStretchRaw (0, 0, 320, 240, cin.width, cin.height, cin.pic);
			if (!cinematicpalette_active)
			{
				CinematicSetPalette(cinematicpalette);
	 			cinematicpalette_active = true;
			}
	    	vid_buffer2=vid_buffer;
	    	SWimp_EndFrame();
      		S_Update_();
  	}

    MainLoop();
    skipit=0;
    if (outbuffer) free(outbuffer);
    outbuffer=0;
  }
  if (outbuffer) free(outbuffer);
  outbuffer=0;
//	cin.pic = SCR_ReadNextFrame ();
//	cinematictime = Sys_Milliseconds ();

	return 1;
}
