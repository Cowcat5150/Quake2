#ifndef _AMIGA_ENDIAN_H
#define _AMIGA_ENDIAN_H

#ifdef __VBCC__

#ifdef __PPC__

short __LittleShort(__reg("r4") short ) =
	"\trlwinm\t0,4,8,16,24\n"
	"\trlwimi\t0,4,24,24,31\n"
	"\textsh\t3,0";

int __LittleLong(__reg("r4") int) =
	"\trlwinm\t3,4,24,0,31\n"
	"\trlwimi\t3,4,8,8,15\n"
	"\trlwimi\t3,4,8,24,31";

float __LittleFloat(__reg("f1") float) =
	"\tstwu\tr1,-32(r1)\n"
	"\tstfs\tf1,24(r1)\n"
	"\tlwz\tr3,24(r1)\n"
	"\tli\tr0,24\n"
	"\tstwbrx\tr3,r1,r0\n"
	"\tlfs\tf1,24(r1)\n"
	"\taddi\tr1,r1,32";

#else // 68k

short __LittleShort(__reg("d0") short ) =
	"\trol.w\t#8,d0";

int __LittleLong(__reg("d0") int) =
	"\trol.w\t#8,d0\n"
	"\tswap\td0\n"
	"\trol.w\t#8,d0";

float __LittleFloat(__reg("fp0") float) =
	"\tfmove.s\tfp0,d0\n"
	"\trol.w\t#8,d0\n"
	"\tswap\td0\n"
	"\trol.w\t#8,d0\n"
	"\tfmove.s\td0,fp0";

#endif

#define LittleShort(x) __LittleShort(x)
#define LittleLong(x) __LittleLong(x)
#define LittleFloat(x) __LittleFloat(x)

#else  // !VBCC

/*
volatile static inline long LittleLong(long l)
{
    volatile long r;
    volatile long d = l;
    __asm volatile(
	  "lwbrx %0,0,%1"
	  : "=r" (r)
	  : "r" (&d)
    );

    return r;
}
*/

#define LittleLong(x) (((uint32_t)(x) << 24 ) | (((uint32_t)(x) & 0xff00) << 8 ) | (((uint32_t)(x) & 0x00ff0000) >> 8 ) | ((uint32_t)(x) >> 24))

/*
volatile static inline short LittleShort(short l)
{
    volatile short r;
    volatile short d = l;
    __asm volatile (
	  "lhbrx %0,0,%1"
	  : "=r" (r)
	  : "r" (&d)
    );

    return r;
}
*/

static inline short LittleShort (short l)
{
	return __builtin_bswap16(l);
}

static inline float LittleFloat(float l)
{
    volatile float r;
    volatile float d = l;
    __asm volatile(
	  "lwbrx %0,0,%1"
	  : "=r" (r)
	  : "r" (&d)
    );

    return r;
}

#endif

#define BigShort(x) (x)
#define BigLong(x) (x)
#define BigFloat(x) (x)

#endif


