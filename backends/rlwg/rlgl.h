#ifndef RAYLIB_BACKENDS_RLWG_RLGL_H
#define RAYLIB_BACKENDS_RLWG_RLGL_H

// This wrapper is only hit when a C++ TU (or a module other than the overlaid
// rcore.c) includes "rlgl.h" while the rlwg backend include dir is on the search
// path. The overlaid rcore.c includes backends/rlwg/rlwg.h directly for the full
// implementation; everything else just needs the stock rlgl declarations plus the
// extra stencil/state entry points rlwg exposes.
#if defined(__cplusplus)
    #include_next <rlgl.h>

extern "C" {
void rlEnableStencilTest(void);
void rlDisableStencilTest(void);
void rlStencilFunc(int func, int ref, int mask);
void rlStencilOp(int fail, int zfail, int zpass);
void rlStencilMask(int mask);
void rlClearStencilBuffer(unsigned int value);
}
#else
    #include "rlwg.h"
#endif

#endif
