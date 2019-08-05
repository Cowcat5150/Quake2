#include <proto/exec.h>

unsigned char id[80];

void main()
{
	CopyMem(0xF00010,id,8);
  printf("%c %c %c %c %c %c %c %c\n",id[0],id[1],id[2],id[3],id[4],id[5],id[6],id[7]);
}

