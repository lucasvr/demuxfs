#ifndef __debug_h
#define __debug_h

#include <ctype.h>

static inline void hexdump(const char *buf, int len)
{
	bool shown_last_ascii = false;
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
		shown_last_ascii = false;
		printf("0x%02x ", buf[x] & 0xff);
		
		if ((x % 16) == 0) {
			printf(" ");
			for (y=x-16; y<=x; ++y)
				printf("%c", isprint(buf[y]) ? buf[y] & 0xff : '.');
			printf("\n%08x    ", counter++ * 16);
			shown_last_ascii = true;
		} else if ((x % 4) == 0)
			printf(" ");
	}

	if (! shown_last_ascii) {
		/* Dump ASCII for last line */
		int spaces = 16 - (len % 16);
		for (x=0; x<spaces; ++x) {
			printf("     ");
			if ((x % 4) == 0)
				printf(" ");
		}
		for (y=len-(len%16); y<=len; ++y)
			printf("%c", isprint(buf[y]) ? buf[y] & 0xff : '.');
	}

	/* Footer */
	printf("\n");
	for (x=0; x<113; ++x)
		printf("*");
	printf("\n");
}

#endif /* __debug_h */
