/*
 * Mock Xilinx type definitions for host-side compilation of the xsdps driver.
 * This header is only for the mock build; target builds use the BSP version.
 */
#ifndef XIL_TYPES_H
#define XIL_TYPES_H

#include <stdint.h>

#define INLINE inline
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t s32;
typedef uintptr_t UINTPTR;
typedef intptr_t INTPTR;

#endif /* XIL_TYPES_H */
