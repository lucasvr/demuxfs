#ifndef __byteops_h
#define __byteops_h

#define CONVERT_TO_16(a,b)       ((((uint16_t)a)<<8) | ((uint8_t)b))
#define CONVERT_TO_24(a,b,c)     ((((uint32_t)a)<<16) | (((uint16_t)b)<<8) | ((uint8_t)c))
#define CONVERT_TO_32(a,b,c,d)   ((((uint32_t)a)<<24) | (((uint32_t)b)<<16) | (((uint16_t)c)<<8) | ((uint8_t)d))
#define CONVERT_TO_40(a,b,c,d,e) ((((uint64_t)a)<<32) | (((uint32_t)b)<<24) | (((uint32_t)c)<<16) | ((uint16_t)d)<<8 | ((uint8_t)e))

#endif /* __byteops_h */
