/* Host-side mock for the Xilinx assertion macro. */
#ifndef XIL_ASSERT_H
#define XIL_ASSERT_H

#include <assert.h>

#define Xil_AssertNonvoid(Expression) assert(Expression)

#endif /* XIL_ASSERT_H */
