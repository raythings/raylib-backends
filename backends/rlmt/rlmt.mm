#define rlEnableStencilTest rlmtBackendEnableStencilTest
#define rlDisableStencilTest rlmtBackendDisableStencilTest
#define rlStencilFunc rlmtBackendStencilFunc
#define rlStencilOp rlmtBackendStencilOp
#define rlStencilMask rlmtBackendStencilMask
#define rlClearStencilBuffer rlmtBackendClearStencilBuffer

#import <TargetConditionals.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#if TARGET_OS_OSX
#import <Cocoa/Cocoa.h>
#else
#import <UIKit/UIKit.h>
#endif

// rlmt is compiled without ARC. Its global Objective-C caches must own the
// dictionaries they store, otherwise mobile autorelease pools can reclaim them.
#define dictionary new
#include "../../../rlmt/src/rlmt.mm"
#undef dictionary

#undef rlEnableStencilTest
#undef rlDisableStencilTest
#undef rlStencilFunc
#undef rlStencilOp
#undef rlStencilMask
#undef rlClearStencilBuffer

extern "C" {
void rlEnableStencilTest(void) { rlmtBackendEnableStencilTest(); }
void rlDisableStencilTest(void) { rlmtBackendDisableStencilTest(); }
void rlStencilFunc(int func, int ref, int mask) { rlmtBackendStencilFunc(func, ref, mask); }
void rlStencilOp(int fail, int zfail, int zpass) { rlmtBackendStencilOp(fail, zfail, zpass); }
void rlStencilMask(int mask) { rlmtBackendStencilMask(mask); }
void rlClearStencilBuffer(unsigned int value) { rlmtBackendClearStencilBuffer(value); }
}
