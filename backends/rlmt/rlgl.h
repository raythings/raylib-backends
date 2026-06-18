#ifndef RAYLIB_BACKENDS_RLMT_RLGL_H
#define RAYLIB_BACKENDS_RLMT_RLGL_H

#if defined(__cplusplus)
    #include_next <rlgl.h>

extern "C" {
void rlEnableStencilTest(void);
void rlDisableStencilTest(void);
void rlStencilFunc(int func, int ref, int mask);
void rlStencilOp(int fail, int zfail, int zpass);
void rlStencilMask(int mask);
void rlClearStencilBuffer(unsigned int value);
void rlColorMask(bool r, bool g, bool b, bool a);
void rlEnableDepthMask(void);
void rlDisableDepthMask(void);
}
#else
    #include "rlmt.h"
#endif

#endif
