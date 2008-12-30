#ifndef __byteops_h
#define __byteops_h

#define CONVERT_TO_16(a,b)       (((a)<<8) | (b))
#define CONVERT_TO_24(a,b,c)     (((a)<<16) | ((b)<<8) | (c))
#define CONVERT_TO_32(a,b,c,d)   (((a)<<24) | ((b)<<16) | ((c)<<8) | (d))
#define CONVERT_TO_40(a,b,c,d,e) ((CONVERT_TO_32(a,b,c,d) << 8) | (e))

#endif /* __byteops_h */
