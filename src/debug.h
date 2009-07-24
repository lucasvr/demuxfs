#ifndef __debug_h
#define __debug_h

#include <ctype.h>

static inline void hexdump(const char *buf, int len)
{
	int x, y, counter = 1;

	/* Header */
	printf("\n");
	for (x=0; x<52; ++x)
		printf("*");
	printf(" Hexdump ");
	for (x=0; x<52; ++x)
		printf("*");
	printf("\n");

	/* Dump */
	printf("00000000    ");
	for (x=1; x<=len; ++x) {
		printf("0x%02x ", buf[x] & 0xff);
		
		if ((x % 16) == 0) {
			printf(" ");
			for (y=x-16; y<=x; ++y)
				printf("%c", isprint(buf[y]) ? buf[y] & 0xff : '.');
			printf("\n%08x    ", counter++ * 16);
		} else if ((x % 4) == 0)
			printf(" ");
	}

	/* Footer */
	printf("\n");
	for (x=0; x<113; ++x)
		printf("*");
	printf("\n");
}

#endif /* __debug_h */
