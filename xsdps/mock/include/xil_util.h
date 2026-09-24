#ifndef XIL_UTIL_H
#define XIL_UTIL_H

#include "xil_types.h"
#include "xstatus.h"

static inline s32 Xil_WaitForEvent(UINTPTR Address, u32 Mask, u32 Value, u32 Timeout)
{
	(void)Address; (void)Mask; (void)Value; (void)Timeout;
	return XST_SUCCESS;
}

static inline s32 Xil_WaitForEvents(UINTPTR Address, u32 Mask, u32 Value,
				    u32 Timeout, u32 *Status)
{
	(void)Address; (void)Mask; (void)Value; (void)Timeout;
	if (Status != NULL) { *Status = 0U; }
	return XST_SUCCESS;
}

#endif /* XIL_UTIL_H */
