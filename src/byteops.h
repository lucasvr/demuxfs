#ifndef __byteops_h
#define __byteops_h

#define MASK_16 0xff00
#define MASK_24 0xff0000
#define MASK_32 0xff000000
#define MASK_40 0xff00000000
#define MASK_48 0xff0000000000
#define MASK_56 0xff000000000000
#define MASK_64 0xff00000000000000

#define CONVERT_TO_16(a,b)             (((((uint16_t)a)<<8)  & MASK_16) | ((uint8_t)b & 0xff))
#define CONVERT_TO_24(a,b,c)           (((((uint32_t)a)<<16) & MASK_24) | CONVERT_TO_16(b,c))
#define CONVERT_TO_32(a,b,c,d)         (((((uint32_t)a)<<24) & MASK_32) | CONVERT_TO_24(b,c,d))
#define CONVERT_TO_40(a,b,c,d,e)       (((((uint64_t)a)<<32) & MASK_40) | CONVERT_TO_32(b,c,d,e))
#define CONVERT_TO_48(a,b,c,d,e,f)     (((((uint64_t)a)<<40) & MASK_48) | CONVERT_TO_40(b,c,d,e,f))
#define CONVERT_TO_56(a,b,c,d,e,f,g)   (((((uint64_t)a)<<48) & MASK_56) | CONVERT_TO_48(b,c,d,e,f,g))
#define CONVERT_TO_64(a,b,c,d,e,f,g,h) (((((uint64_t)a)<<56) & MASK_64) | CONVERT_TO_56(b,c,d,e,f,g,h))

#endif /* __byteops_h */
