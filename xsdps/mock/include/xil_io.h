#ifndef XIL_IO_H
#define XIL_IO_H

#include "xil_types.h"

static inline u8 Xil_In8(UINTPTR Address) { (void)Address; return 0U; }
static inline u16 Xil_In16(UINTPTR Address) { (void)Address; return 0U; }
static inline u32 Xil_In32(UINTPTR Address) { (void)Address; return 0U; }
static inline u64 Xil_In64(UINTPTR Address) { (void)Address; return 0U; }
static inline void Xil_Out8(UINTPTR Address, u8 Value) { (void)Address; (void)Value; }
static inline void Xil_Out16(UINTPTR Address, u16 Value) { (void)Address; (void)Value; }
static inline void Xil_Out32(UINTPTR Address, u32 Value) { (void)Address; (void)Value; }
static inline void Xil_Out64(UINTPTR Address, u64 Value) { (void)Address; (void)Value; }

#endif /* XIL_IO_H */
