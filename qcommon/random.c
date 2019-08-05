// Random routines

#include <stdlib.h>

static unsigned long holdrand = 0x89abcdef;

// Returns a float min <= x < max (exclusive; will get max - 0.00001; but never max

float flrand(float min, float max)
{
    	float   result;

    	holdrand = (holdrand * 214013L) + 2531011L;
    	result = (float)(holdrand >> 17);		// 0 - 32767 range
    	result = ((result * (max - min)) / 32768.0F) + min;

    	return(result);
}




