#include <stdlib.h>
#include "GLCD.h"

unsigned char* MakeBdybmp(unsigned short color)
{
	int i = 0;
	unsigned char* bmp = (unsigned char*) malloc(sizeof(30*30));
	for(i = 0; i < 900; i++)
	{
		bmp[i] = color;
	}
	return bmp;
}

body* MakeBody(unsigned int x, unsigned int y, unsigned short color)
{
	body* bodyptr = (body*) malloc(sizeof(body));
	body->x = x;
	body->y = y;
	body->bmp = MakeBdybmp(color);
	
	return body;
}
