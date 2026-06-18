/**********************************************************************************************
*
*   rlwg_impl2.h - rlwg backend implementation, part 2 (matrices, textures, FBOs, state)
*   Included at the bottom of rlwg_impl.h.
*
**********************************************************************************************/

#ifndef RLWG_IMPL2_H
#define RLWG_IMPL2_H

#define RLWG_DEG2RAD (3.14159265358979323846f/180.0f)

//----------------------------------------------------------------------------------
// Matrix stack (rlgl-accurate; mvp = modelview*projection is formed at flush)
//----------------------------------------------------------------------------------
void rlMatrixMode(int mode)
{
    if (mode == RL_PROJECTION) RLWG.State.currentMatrix = &RLWG.State.projection;
    else if (mode == RL_MODELVIEW) RLWG.State.currentMatrix = &RLWG.State.modelview;
    // RL_TEXTURE: raylib keeps using modelview pointer; no texture matrix in core path
    RLWG.State.currentMatrixMode = mode;
}

void rlPushMatrix(void)
{
    if (RLWG.State.stackCounter >= RL_MAX_MATRIX_STACK_SIZE) { TRACELOG(RL_LOG_ERROR, "RLWG: matrix stack overflow"); return; }
    if (RLWG.State.currentMatrixMode == RL_MODELVIEW)
    {
        RLWG.State.transformRequired = true;
        RLWG.State.currentMatrix = &RLWG.State.transform;
    }
    RLWG.State.stack[RLWG.State.stackCounter] = *RLWG.State.currentMatrix;
    RLWG.State.stackCounter++;
}

void rlPopMatrix(void)
{
    if (RLWG.State.stackCounter > 0)
    {
        Matrix mat = RLWG.State.stack[RLWG.State.stackCounter - 1];
        *RLWG.State.currentMatrix = mat;
        RLWG.State.stackCounter--;
    }
    if ((RLWG.State.stackCounter == 0) && (RLWG.State.currentMatrixMode == RL_MODELVIEW))
    {
        RLWG.State.currentMatrix = &RLWG.State.modelview;
        RLWG.State.transformRequired = false;
    }
}

void rlLoadIdentity(void) { *RLWG.State.currentMatrix = rlwgMatrixIdentity(); }

void rlTranslatef(float x, float y, float z)
{
    Matrix t = { 1,0,0,x, 0,1,0,y, 0,0,1,z, 0,0,0,1 };
    *RLWG.State.currentMatrix = rlwgMatrixMultiply(t, *RLWG.State.currentMatrix);
}

void rlRotatef(float angle, float x, float y, float z)
{
    Matrix r = rlwgMatrixIdentity();
    float lengthSq = x*x + y*y + z*z;
    if (lengthSq != 1.0f && lengthSq != 0.0f) { float il = 1.0f/sqrtf(lengthSq); x*=il; y*=il; z*=il; }
    float s = sinf(angle*RLWG_DEG2RAD);
    float c = cosf(angle*RLWG_DEG2RAD);
    float t = 1.0f - c;
    r.m0 = x*x*t + c;    r.m4 = x*y*t - z*s;  r.m8  = x*z*t + y*s;
    r.m1 = y*x*t + z*s;  r.m5 = y*y*t + c;    r.m9  = y*z*t - x*s;
    r.m2 = z*x*t - y*s;  r.m6 = z*y*t + x*s;  r.m10 = z*z*t + c;
    *RLWG.State.currentMatrix = rlwgMatrixMultiply(r, *RLWG.State.currentMatrix);
}

void rlScalef(float x, float y, float z)
{
    Matrix s = { x,0,0,0, 0,y,0,0, 0,0,z,0, 0,0,0,1 };
    *RLWG.State.currentMatrix = rlwgMatrixMultiply(s, *RLWG.State.currentMatrix);
}

void rlMultMatrixf(const float *matf)
{
    Matrix m = { matf[0],matf[4],matf[8],matf[12],
                 matf[1],matf[5],matf[9],matf[13],
                 matf[2],matf[6],matf[10],matf[14],
                 matf[3],matf[7],matf[11],matf[15] };
    *RLWG.State.currentMatrix = rlwgMatrixMultiply(m, *RLWG.State.currentMatrix);
}

void rlFrustum(double left, double right, double bottom, double top, double znear, double zfar)
{
    Matrix mf = {0};
    double rl = right-left, tb = top-bottom, fn = zfar-znear;
    mf.m0 = (float)(znear*2.0/rl);
    mf.m5 = (float)(znear*2.0/tb);
    mf.m8 = (float)((right+left)/rl);
    mf.m9 = (float)((top+bottom)/tb);
    mf.m10 = (float)(-(zfar+znear)/fn);
    mf.m11 = -1.0f;
    mf.m14 = (float)(-(zfar*znear*2.0)/fn);
    *RLWG.State.currentMatrix = rlwgMatrixMultiply(*RLWG.State.currentMatrix, mf);
}

void rlOrtho(double left, double right, double bottom, double top, double znear, double zfar)
{
    Matrix mo = {0};
    double rl = right-left, tb = top-bottom, fn = zfar-znear;
    mo.m0 = (float)(2.0/rl);
    mo.m5 = (float)(2.0/tb);
    mo.m10 = (float)(-2.0/fn);
    mo.m12 = (float)(-(left+right)/rl);
    mo.m13 = (float)(-(top+bottom)/tb);
    mo.m14 = (float)(-(zfar+znear)/fn);
    mo.m15 = 1.0f;
    *RLWG.State.currentMatrix = rlwgMatrixMultiply(*RLWG.State.currentMatrix, mo);
}

void rlViewport(int x, int y, int width, int height)
{
    if (RLWG.passOpen)
        wgpuRenderPassEncoderSetViewport(RLWG.pass, (float)x, (float)y, (float)width, (float)height, 0.0f, 1.0f);
}

void rlSetClipPlanes(double nearPlane, double farPlane) { RLWG_CullDistanceNear = nearPlane; RLWG_CullDistanceFar = farPlane; }
double rlGetCullDistanceNear(void) { return RLWG_CullDistanceNear; }
double rlGetCullDistanceFar(void) { return RLWG_CullDistanceFar; }

Matrix rlGetMatrixModelview(void) { return RLWG.State.modelview; }
Matrix rlGetMatrixProjection(void) { return RLWG.State.projection; }
Matrix rlGetMatrixTransform(void) { return RLWG.State.transform; }
Matrix rlGetMatrixProjectionStereo(int eye) { return RLWG.State.projectionStereo[eye]; }
Matrix rlGetMatrixViewOffsetStereo(int eye) { return RLWG.State.viewOffsetStereo[eye]; }
void rlSetMatrixProjection(Matrix proj) { RLWG.State.projection = proj; }
void rlSetMatrixModelview(Matrix view) { RLWG.State.modelview = view; }
void rlSetMatrixProjectionStereo(Matrix right, Matrix left) { RLWG.State.projectionStereo[0]=right; RLWG.State.projectionStereo[1]=left; }
void rlSetMatrixViewOffsetStereo(Matrix right, Matrix left) { RLWG.State.viewOffsetStereo[0]=right; RLWG.State.viewOffsetStereo[1]=left; }

//----------------------------------------------------------------------------------
// Clear / frame buffers
//----------------------------------------------------------------------------------
void rlClearColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    RLWG.clearColor = (WGPUColor){ r/255.0, g/255.0, b/255.0, a/255.0 };
}
void rlClearScreenBuffers(void) { /* clear happens via loadOp=Clear on first pass of the frame */ }
void rlCheckErrors(void) {}

//----------------------------------------------------------------------------------
// Render state
//----------------------------------------------------------------------------------
void rlEnableColorBlend(void) {}
void rlDisableColorBlend(void) {}
void rlEnableDepthTest(void) { if (!RLWG.State.depthTestEnabled) rlwgFlush(); RLWG.State.depthTestEnabled = true; }
void rlDisableDepthTest(void) { if (RLWG.State.depthTestEnabled) rlwgFlush(); RLWG.State.depthTestEnabled = false; }
void rlEnableDepthMask(void) { RLWG.State.depthWriteEnabled = true; }
void rlDisableDepthMask(void) { RLWG.State.depthWriteEnabled = false; }
void rlEnableBackfaceCulling(void) { if (!RLWG.State.cullEnabled) rlwgFlush(); RLWG.State.cullEnabled = true; }
void rlDisableBackfaceCulling(void) { if (RLWG.State.cullEnabled) rlwgFlush(); RLWG.State.cullEnabled = false; }
void rlSetCullFace(int mode) { (void)mode; }
void rlColorMask(bool r, bool g, bool b, bool a) { RLWG.State.colorMaskR=r; RLWG.State.colorMaskG=g; RLWG.State.colorMaskB=b; RLWG.State.colorMaskA=a; }

void rlEnableScissorTest(void) {}
void rlDisableScissorTest(void) { if (RLWG.passOpen) wgpuRenderPassEncoderSetScissorRect(RLWG.pass, 0, 0, (uint32_t)RLWG.State.framebufferWidth, (uint32_t)RLWG.State.framebufferHeight); }
void rlScissor(int x, int y, int width, int height)
{
    if (x < 0) { width += x; x = 0; }
    if (y < 0) { height += y; y = 0; }
    if (width < 0) width = 0;
    if (height < 0) height = 0;
    if (RLWG.passOpen) wgpuRenderPassEncoderSetScissorRect(RLWG.pass, (uint32_t)x, (uint32_t)y, (uint32_t)width, (uint32_t)height);
}

void rlEnableWireMode(void) {}     // WebGPU has no polygon-line fill mode
void rlEnablePointMode(void) {}
void rlDisableWireMode(void) {}
void rlSetLineWidth(float width) { RLWG_LineWidth = width; }  // WebGPU line width is always 1
float rlGetLineWidth(void) { return RLWG_LineWidth; }
void rlEnableSmoothLines(void) {}
void rlDisableSmoothLines(void) {}

void rlEnableStereoRender(void) { RLWG.State.stereoRender = true; }
void rlDisableStereoRender(void) { RLWG.State.stereoRender = false; }
bool rlIsStereoRenderEnabled(void) { return RLWG.State.stereoRender; }

void rlSetBlendMode(int mode) { if (RLWG.State.currentBlendMode != mode) rlwgFlush(); RLWG.State.currentBlendMode = mode; }
void rlSetBlendFactors(int s, int d, int eq) { (void)s; (void)d; (void)eq; }
void rlSetBlendFactorsSeparate(int a, int b, int c, int d, int e, int f) { (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; }

// Stencil (no stencil buffer in the default depth target; no-ops keep callers happy)
void rlEnableStencilTest(void) {}
void rlDisableStencilTest(void) {}
void rlStencilFunc(int func, int ref, int mask) { (void)func;(void)ref;(void)mask; }
void rlStencilOp(int fail, int zfail, int zpass) { (void)fail;(void)zfail;(void)zpass; }
void rlStencilMask(int mask) { (void)mask; }
void rlClearStencilBuffer(unsigned int value) { (void)value; }

//----------------------------------------------------------------------------------
// Texture / framebuffer / shader binding state
//----------------------------------------------------------------------------------
void rlActiveTextureSlot(int slot) { (void)slot; }
void rlEnableTexture(unsigned int id) { rlSetTexture(id); }
void rlDisableTexture(void) { RLWG.State.currentTextureId = RLWG.State.defaultTextureId; }
void rlEnableTextureCubemap(unsigned int id) { (void)id; }
void rlDisableTextureCubemap(void) {}
void rlTextureParameters(unsigned int id, int param, int value) { (void)id;(void)param;(void)value; }
void rlCubemapParameters(unsigned int id, int param, int value) { (void)id;(void)param;(void)value; }

void rlEnableFramebuffer(unsigned int id) { rlwgFlush(); RLWG.activeFramebufferId = id; }
void rlDisableFramebuffer(void) { rlwgFlush(); RLWG.activeFramebufferId = 0; }
unsigned int rlGetActiveFramebuffer(void) { return RLWG.activeFramebufferId; }
void rlActiveDrawBuffers(int count) { (void)count; }
void rlBindFramebuffer(unsigned int target, unsigned int framebuffer) { (void)target; RLWG.activeFramebufferId = framebuffer; }
void rlBlitFramebuffer(int sx,int sy,int sw,int sh,int dx,int dy,int dw,int dh,int mask) { (void)sx;(void)sy;(void)sw;(void)sh;(void)dx;(void)dy;(void)dw;(void)dh;(void)mask; }

//----------------------------------------------------------------------------------
// Textures
//----------------------------------------------------------------------------------
// WebGPU has no luminance/luminance-alpha formats and no per-texture sampler swizzle
// like GL's GL_LUMINANCE(_ALPHA). To stay 1:1 with GL - where a GRAYSCALE texture
// samples as (L,L,L,1) and GRAY_ALPHA as (L,L,L,A) - expand those formats to RGBA8 on
// upload. Returns a malloc'd RGBA8 buffer the caller must free, or NULL if no expansion
// is needed (then the original data/format are used as-is).
static unsigned char *rlwgExpandToRGBA8(const void *data, int width, int height, int format)
{
    if (!data) return NULL;
    const unsigned char *src = (const unsigned char *)data;
    int count = width*height;
    if (format == RL_PIXELFORMAT_UNCOMPRESSED_GRAYSCALE)
    {
        unsigned char *out = (unsigned char *)RL_MALLOC((size_t)count*4);
        for (int i = 0; i < count; i++) { unsigned char l = src[i]; out[i*4+0]=l; out[i*4+1]=l; out[i*4+2]=l; out[i*4+3]=255; }
        return out;
    }
    if (format == RL_PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA)
    {
        unsigned char *out = (unsigned char *)RL_MALLOC((size_t)count*4);
        for (int i = 0; i < count; i++) { unsigned char l = src[i*2+0], a = src[i*2+1]; out[i*4+0]=l; out[i*4+1]=l; out[i*4+2]=l; out[i*4+3]=a; }
        return out;
    }
    return NULL;
}

unsigned int rlLoadTexture(const void *data, int width, int height, int format, int mipmapCount)
{
    (void)mipmapCount;
    if (!RLWG.device) return 0;
    RLWGTextureRecord *t = rlwgAllocTexture();
    t->id = ++RLWG.nextTextureId;
    t->width = width; t->height = height; t->format = format; t->mipmaps = 1;

    // Expand luminance formats to RGBA8 (see rlwgExpandToRGBA8).
    unsigned char *expanded = rlwgExpandToRGBA8(data, width, height, format);
    if (expanded) { data = expanded; format = RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8; t->format = format; }

    WGPUTextureFormat wfmt = rlwgToWGPUFormat(format);
    WGPUTextureDescriptor td = {0};
    td.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    if (RLWG.activeFramebufferId != 0 || data == NULL) td.usage |= WGPUTextureUsage_RenderAttachment;
    td.dimension = WGPUTextureDimension_2D;
    td.size.width = (uint32_t)width; td.size.height = (uint32_t)height; td.size.depthOrArrayLayers = 1;
    td.format = wfmt; td.mipLevelCount = 1; td.sampleCount = 1;
    t->texture = wgpuDeviceCreateTexture(RLWG.device, &td);

    WGPUTextureViewDescriptor vd = {0};
    vd.format = wfmt; vd.dimension = WGPUTextureViewDimension_2D; vd.mipLevelCount = 1; vd.arrayLayerCount = 1;
    t->view = wgpuTextureCreateView(t->texture, &vd);

    WGPUSamplerDescriptor sd = {0};
    sd.addressModeU = WGPUAddressMode_ClampToEdge;
    sd.addressModeV = WGPUAddressMode_ClampToEdge;
    sd.addressModeW = WGPUAddressMode_ClampToEdge;
    sd.magFilter = WGPUFilterMode_Nearest;
    sd.minFilter = WGPUFilterMode_Nearest;
    sd.mipmapFilter = WGPUMipmapFilterMode_Nearest;
    sd.maxAnisotropy = 1;
    t->sampler = wgpuDeviceCreateSampler(RLWG.device, &sd);

    if (data)
    {
        int bpp = rlwgPixelByteSize(format);
        WGPUTexelCopyTextureInfo dst = {0};
        dst.texture = t->texture;
        dst.mipLevel = 0;
        WGPUTexelCopyBufferLayout layout = {0};
        layout.offset = 0;
        layout.bytesPerRow = (uint32_t)(width*bpp);
        layout.rowsPerImage = (uint32_t)height;
        WGPUExtent3D ext = { (uint32_t)width, (uint32_t)height, 1 };
        wgpuQueueWriteTexture(RLWG.queue, &dst, data, (size_t)width*height*bpp, &layout, &ext);
    }
    if (expanded) RL_FREE(expanded);
    return t->id;
}

unsigned int rlLoadTextureDepth(int width, int height, bool useRenderBuffer)
{
    (void)useRenderBuffer;
    RLWGTextureRecord *t = rlwgAllocTexture();
    t->id = ++RLWG.nextTextureId;
    t->width = width; t->height = height; t->format = 0; t->mipmaps = 1;
    WGPUTextureDescriptor td = {0};
    td.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding;
    td.dimension = WGPUTextureDimension_2D;
    td.size.width = (uint32_t)width; td.size.height = (uint32_t)height; td.size.depthOrArrayLayers = 1;
    td.format = RLWG.depthFormat; td.mipLevelCount = 1; td.sampleCount = 1;
    t->texture = wgpuDeviceCreateTexture(RLWG.device, &td);
    WGPUTextureViewDescriptor vd = {0};
    vd.format = RLWG.depthFormat; vd.dimension = WGPUTextureViewDimension_2D; vd.mipLevelCount = 1; vd.arrayLayerCount = 1;
    t->view = wgpuTextureCreateView(t->texture, &vd);
    return t->id;
}

unsigned int rlLoadTextureCubemap(const void *data, int size, int format, int mipmapCount)
{
    (void)data; (void)size; (void)format; (void)mipmapCount;
    TRACELOG(RL_LOG_WARNING, "RLWG: cubemap textures not yet implemented");
    return 0;
}

void rlUpdateTexture(unsigned int id, int offsetX, int offsetY, int width, int height, int format, const void *data)
{
    RLWGTextureRecord *t = rlwgGetTexture(id);
    if (!t || !data) return;
    unsigned char *expanded = rlwgExpandToRGBA8(data, width, height, format);
    if (expanded) { data = expanded; format = RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8; }
    int bpp = rlwgPixelByteSize(format);
    WGPUTexelCopyTextureInfo dst = {0};
    dst.texture = t->texture; dst.mipLevel = 0;
    dst.origin.x = (uint32_t)offsetX; dst.origin.y = (uint32_t)offsetY;
    WGPUTexelCopyBufferLayout layout = {0};
    layout.bytesPerRow = (uint32_t)(width*bpp);
    layout.rowsPerImage = (uint32_t)height;
    WGPUExtent3D ext = { (uint32_t)width, (uint32_t)height, 1 };
    wgpuQueueWriteTexture(RLWG.queue, &dst, data, (size_t)width*height*bpp, &layout, &ext);
    if (expanded) RL_FREE(expanded);
}

void rlUnloadTexture(unsigned int id)
{
    RLWGTextureRecord *t = rlwgGetTexture(id);
    if (!t) return;
    if (t->sampler) wgpuSamplerRelease(t->sampler);
    if (t->view) wgpuTextureViewRelease(t->view);
    if (t->texture) wgpuTextureRelease(t->texture);
    t->used = false;
}

void rlGenTextureMipmaps(unsigned int id, int width, int height, int format, int *mipmaps)
{
    (void)id;(void)width;(void)height;(void)format;
    if (mipmaps) *mipmaps = 1; // mipmap generation via blit pass: TODO
}

void rlGetGlTextureFormats(int format, unsigned int *glInternalFormat, unsigned int *glFormat, unsigned int *glType)
{
    if (glInternalFormat) *glInternalFormat = 0;
    if (glFormat) *glFormat = 0;
    if (glType) *glType = 0;
    (void)format;
}

const char *rlGetPixelFormatName(unsigned int format)
{
    switch (format)
    {
        case RL_PIXELFORMAT_UNCOMPRESSED_GRAYSCALE: return "GRAYSCALE";
        case RL_PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA: return "GRAY_ALPHA";
        case RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8: return "R8G8B8";
        case RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8: return "R8G8B8A8";
        case RL_PIXELFORMAT_UNCOMPRESSED_R32: return "R32";
        case RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32A32: return "R32G32B32A32";
        default: return "UNKNOWN";
    }
}

void *rlReadTexturePixels(unsigned int id, int width, int height, int format)
{
    (void)id;(void)width;(void)height;(void)format;
    TRACELOG(RL_LOG_WARNING, "RLWG: rlReadTexturePixels needs async buffer map; not implemented");
    return NULL;
}

unsigned char *rlReadScreenPixels(int width, int height)
{
    (void)width;(void)height;
    TRACELOG(RL_LOG_WARNING, "RLWG: rlReadScreenPixels not implemented");
    return NULL;
}

unsigned int rlGetTextureIdDefault(void) { return RLWG.State.defaultTextureId; }

//----------------------------------------------------------------------------------
// Framebuffers (render textures)
//----------------------------------------------------------------------------------
static RLWGFramebufferRecord *rlwgAllocFramebuffer(void)
{
    for (int i = 0; i < RLWG.framebufferCapacity; i++)
        if (!RLWG.framebuffers[i].used) { memset(&RLWG.framebuffers[i], 0, sizeof(RLWGFramebufferRecord)); RLWG.framebuffers[i].used = true; return &RLWG.framebuffers[i]; }
    int old = RLWG.framebufferCapacity;
    int nc = old > 0 ? old*2 : 16;
    RLWG.framebuffers = (RLWGFramebufferRecord *)RL_REALLOC(RLWG.framebuffers, (size_t)nc*sizeof(RLWGFramebufferRecord));
    memset(&RLWG.framebuffers[old], 0, (size_t)(nc-old)*sizeof(RLWGFramebufferRecord));
    RLWG.framebufferCapacity = nc;
    RLWG.framebuffers[old].used = true;
    return &RLWG.framebuffers[old];
}
static RLWGFramebufferRecord *rlwgGetFramebuffer(unsigned int id)
{
    for (int i = 0; i < RLWG.framebufferCapacity; i++)
        if (RLWG.framebuffers[i].used && RLWG.framebuffers[i].id == id) return &RLWG.framebuffers[i];
    return NULL;
}

unsigned int rlLoadFramebuffer(void)
{
    RLWGFramebufferRecord *f = rlwgAllocFramebuffer();
    f->id = ++RLWG.nextFramebufferId;
    return f->id;
}

void rlFramebufferAttach(unsigned int fboId, unsigned int texId, int attachType, int texType, int mipLevel)
{
    (void)texType; (void)mipLevel;
    RLWGFramebufferRecord *f = rlwgGetFramebuffer(fboId);
    if (!f) return;
    if (attachType == RL_ATTACHMENT_DEPTH) f->depthTex = texId;
    else f->colorTex = texId;
    RLWGTextureRecord *t = rlwgGetTexture(f->colorTex ? f->colorTex : texId);
    if (t) { f->width = t->width; f->height = t->height; }
}

bool rlFramebufferComplete(unsigned int id)
{
    RLWGFramebufferRecord *f = rlwgGetFramebuffer(id);
    return (f != NULL) && (f->colorTex != 0);
}

void rlUnloadFramebuffer(unsigned int id)
{
    RLWGFramebufferRecord *f = rlwgGetFramebuffer(id);
    if (f) f->used = false;
}

//----------------------------------------------------------------------------------
// Vertex arrays / buffers (model draw path)
//----------------------------------------------------------------------------------
unsigned int rlLoadVertexArray(void) { return ++RLWG.nextVertexBufferId; } // VAO id placeholder
bool rlEnableVertexArray(unsigned int vaoId) { (void)vaoId; return true; }
void rlDisableVertexArray(void) {}
void rlUnloadVertexArray(unsigned int vaoId) { (void)vaoId; }

unsigned int rlLoadVertexBuffer(const void *buffer, int size, bool dynamic)
{
    (void)dynamic;
    RLWGVertexBufferRecord *vb = rlwgAllocVB();
    vb->id = ++RLWG.nextVertexBufferId;
    vb->size = (size_t)size;
    vb->element = false;
    WGPUBufferDescriptor bd = {0};
    bd.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
    bd.size = (size_t)size;
    vb->buffer = wgpuDeviceCreateBuffer(RLWG.device, &bd);
    if (buffer) wgpuQueueWriteBuffer(RLWG.queue, vb->buffer, 0, buffer, (size_t)size);
    return vb->id;
}

unsigned int rlLoadVertexBufferElement(const void *buffer, int size, bool dynamic)
{
    (void)dynamic;
    RLWGVertexBufferRecord *vb = rlwgAllocVB();
    vb->id = ++RLWG.nextVertexBufferId;
    vb->size = (size_t)size;
    vb->element = true;
    WGPUBufferDescriptor bd = {0};
    bd.usage = WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst;
    bd.size = (size_t)size;
    vb->buffer = wgpuDeviceCreateBuffer(RLWG.device, &bd);
    if (buffer) wgpuQueueWriteBuffer(RLWG.queue, vb->buffer, 0, buffer, (size_t)size);
    return vb->id;
}

void rlUpdateVertexBuffer(unsigned int bufferId, const void *data, int dataSize, int offset)
{
    RLWGVertexBufferRecord *vb = rlwgGetVB(bufferId);
    if (vb && data) wgpuQueueWriteBuffer(RLWG.queue, vb->buffer, (uint64_t)offset, data, (size_t)dataSize);
}
void rlUpdateVertexBufferElements(unsigned int id, const void *data, int dataSize, int offset) { rlUpdateVertexBuffer(id, data, dataSize, offset); }

void rlUnloadVertexBuffer(unsigned int vboId)
{
    RLWGVertexBufferRecord *vb = rlwgGetVB(vboId);
    if (vb) { if (vb->buffer) wgpuBufferRelease(vb->buffer); vb->used = false; }
}

void rlEnableVertexBuffer(unsigned int id) { (void)id; }
void rlDisableVertexBuffer(void) {}
void rlEnableVertexBufferElement(unsigned int id) { (void)id; }
void rlDisableVertexBufferElement(void) {}
void rlSetVertexAttribute(unsigned int index, int compSize, int type, bool normalized, int stride, const void *pointer) { (void)index;(void)compSize;(void)type;(void)normalized;(void)stride;(void)pointer; }
void rlSetVertexAttributeDivisor(unsigned int index, int divisor) { (void)index;(void)divisor; }
void rlSetVertexAttributeDefault(int locIndex, const void *value, int attribType, int count) { (void)locIndex;(void)value;(void)attribType;(void)count; }
void rlEnableVertexAttribute(unsigned int index) { (void)index; }
void rlDisableVertexAttribute(unsigned int index) { (void)index; }

// NOTE: the generic VAO model-draw path (rlDrawVertexArray*) requires per-mesh
// pipelines with the mesh's own vertex layout. The immediate-mode batch path
// (rlBegin/rlVertex - used by shapes, text, and DrawCube/DrawModel-via-batch) is
// fully supported. Arbitrary-layout mesh VAO draws are a follow-up (see DOCS).
void rlDrawVertexArray(int offset, int count) { (void)offset;(void)count; }
void rlDrawVertexArrayElements(int offset, int count, const void *buffer) { (void)offset;(void)count;(void)buffer; }
void rlDrawVertexArrayInstanced(int offset, int count, int instances) { (void)offset;(void)count;(void)instances; }
void rlDrawVertexArrayElementsInstanced(int offset, int count, const void *buffer, int instances) { (void)offset;(void)count;(void)buffer;(void)instances; }

//----------------------------------------------------------------------------------
// Compute / SSBO (WebGPU supports these; full wiring is a follow-up)
//----------------------------------------------------------------------------------
unsigned int rlLoadComputeShaderProgram(unsigned int shaderId) { (void)shaderId; return 0; }
void rlComputeShaderDispatch(unsigned int x, unsigned int y, unsigned int z) { (void)x;(void)y;(void)z; }
unsigned int rlLoadShaderBuffer(unsigned int size, const void *data, int usageHint) { (void)size;(void)data;(void)usageHint; return 0; }
void rlUnloadShaderBuffer(unsigned int ssboId) { (void)ssboId; }
void rlUpdateShaderBuffer(unsigned int id, const void *data, unsigned int dataSize, unsigned int offset) { (void)id;(void)data;(void)dataSize;(void)offset; }
void rlBindShaderBuffer(unsigned int id, unsigned int index) { (void)id;(void)index; }
void rlReadShaderBuffer(unsigned int id, void *dest, unsigned int count, unsigned int offset) { (void)id;(void)dest;(void)count;(void)offset; }
void rlCopyShaderBuffer(unsigned int d, unsigned int s, unsigned int doff, unsigned int soff, unsigned int count) { (void)d;(void)s;(void)doff;(void)soff;(void)count; }
unsigned int rlGetShaderBufferSize(unsigned int id) { (void)id; return 0; }
void rlBindImageTexture(unsigned int id, unsigned int index, int format, bool readonly) { (void)id;(void)index;(void)format;(void)readonly; }

//----------------------------------------------------------------------------------
// Quick draws / misc getters
//----------------------------------------------------------------------------------
void rlLoadDrawCube(void) {}
void rlLoadDrawQuad(void) {}

void rlSetFramebufferWidth(int width) { RLWG.State.framebufferWidth = width; }
int rlGetFramebufferWidth(void) { return RLWG.State.framebufferWidth; }
void rlSetFramebufferHeight(int height) { RLWG.State.framebufferHeight = height; }
int rlGetFramebufferHeight(void) { return RLWG.State.framebufferHeight; }

#endif // RLWG_IMPL2_H
