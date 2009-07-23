#ifndef __byteops_h
#define __byteops_h

#define CONVERT_TO_16(a,b)       (((((uint16_t)a)<<8) & 0xff00) | ((uint8_t)b & 0xff))
#define CONVERT_TO_24(a,b,c)     (((((uint32_t)a)<<16) & 0xff0000) | CONVERT_TO_16(b,c))
#define CONVERT_TO_32(a,b,c,d)   (((((uint32_t)a)<<24) & 0xff000000) | CONVERT_TO_24(b,c,d))
#define CONVERT_TO_40(a,b,c,d,e) (((((uint64_t)a)<<32) & 0xff00000000) | CONVERT_TO_32(b,c,d,e))

#endif /* __byteops_h */
