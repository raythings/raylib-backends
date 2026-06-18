/**********************************************************************************************
*
*   rlwg_impl.h - rlwg backend implementation (included at the bottom of rlwg.h)
*
*   Split out only to keep rlwg.h's declaration section readable. All symbols here are
*   part of the single overlaid-rcore translation unit; nothing is `static` that rcore
*   needs to link against externally.
*
**********************************************************************************************/

#ifndef RLWG_IMPL_H
#define RLWG_IMPL_H

//----------------------------------------------------------------------------------
// Internal forward decls
//----------------------------------------------------------------------------------
static void rlwgFlush(void);
static void rlwgBeginPassIfNeeded(void);
static unsigned int rlwgLoadDefaultShader(void);
static WGPURenderPipeline rlwgGetPipeline(RLWGShaderRecord *sh, int topology);
static void rlwgEnsureDepth(int w, int h);

//----------------------------------------------------------------------------------
// Platform hooks (called by the web platform layer before rlglInit completes)
//----------------------------------------------------------------------------------
void rlwgSetDevice(WGPUDevice device, WGPUQueue queue)
{
    RLWG.device = device;
    RLWG.queue = queue ? queue : (device ? wgpuDeviceGetQueue(device) : NULL);
}

void rlwgSetSurface(WGPUSurface surface) { RLWG.surface = surface; }

void rlwgResize(int width, int height)
{
    RLWG.State.framebufferWidth = width;
    RLWG.State.framebufferHeight = height;
    if (RLWG.device && RLWG.surface)
    {
        WGPUSurfaceConfiguration cfg = {0};
        cfg.device = RLWG.device;
        cfg.format = RLWG.surfaceFormat;
        cfg.usage = WGPUTextureUsage_RenderAttachment;
        cfg.width = (uint32_t)width;
        cfg.height = (uint32_t)height;
        cfg.presentMode = RLWG.vSync ? WGPUPresentMode_Fifo : WGPUPresentMode_Immediate;
        cfg.alphaMode = WGPUCompositeAlphaMode_Opaque;
        wgpuSurfaceConfigure(RLWG.surface, &cfg);
    }
    rlwgEnsureDepth(width, height);
}

//----------------------------------------------------------------------------------
// Depth backbuffer
//----------------------------------------------------------------------------------
static void rlwgEnsureDepth(int w, int h)
{
    if (w <= 0 || h <= 0) return;
    if (RLWG.depthTexture && RLWG.depthW == w && RLWG.depthH == h) return;
    if (RLWG.depthTextureView) { wgpuTextureViewRelease(RLWG.depthTextureView); RLWG.depthTextureView = NULL; }
    if (RLWG.depthTexture) { wgpuTextureRelease(RLWG.depthTexture); RLWG.depthTexture = NULL; }

    WGPUTextureDescriptor td = {0};
    td.usage = WGPUTextureUsage_RenderAttachment;
    td.dimension = WGPUTextureDimension_2D;
    td.size.width = (uint32_t)w; td.size.height = (uint32_t)h; td.size.depthOrArrayLayers = 1;
    td.format = RLWG.depthFormat;
    td.mipLevelCount = 1; td.sampleCount = 1;
    RLWG.depthTexture = wgpuDeviceCreateTexture(RLWG.device, &td);
    WGPUTextureViewDescriptor vd = {0};
    vd.format = RLWG.depthFormat;
    vd.dimension = WGPUTextureViewDimension_2D;
    vd.mipLevelCount = 1; vd.arrayLayerCount = 1;
    RLWG.depthTextureView = wgpuTextureCreateView(RLWG.depthTexture, &vd);
    RLWG.depthW = w; RLWG.depthH = h;
}

//----------------------------------------------------------------------------------
// WebGPU instance/adapter/device/surface acquisition (browser / emdawnwebgpu)
//----------------------------------------------------------------------------------
#if defined(__EMSCRIPTEN__)
typedef struct { bool done; WGPUAdapter adapter; } RLWGAdapterReq;
typedef struct { bool done; WGPUDevice device; } RLWGDeviceReq;

static void rlwgOnAdapter(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView msg, void *u1, void *u2)
{
    (void)msg; (void)u2;
    RLWGAdapterReq *r = (RLWGAdapterReq *)u1;
    r->adapter = (status == WGPURequestAdapterStatus_Success) ? adapter : NULL;
    r->done = true;
}
static void rlwgOnDevice(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView msg, void *u1, void *u2)
{
    (void)msg; (void)u2;
    RLWGDeviceReq *r = (RLWGDeviceReq *)u1;
    r->device = (status == WGPURequestDeviceStatus_Success) ? device : NULL;
    r->done = true;
}

static void rlwgAcquireDevice(const char *canvasSelector)
{
    RLWG.instance = wgpuCreateInstance(NULL);
    if (!RLWG.instance) { TRACELOG(RL_LOG_FATAL, "RLWG: wgpuCreateInstance failed"); return; }

    // Surface from the HTML canvas (needed before requesting a compatible adapter).
    WGPUEmscriptenSurfaceSourceCanvasHTMLSelector sel = {0};
    sel.chain.sType = WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector;
    sel.selector = rlwgStr(canvasSelector);
    WGPUSurfaceDescriptor sd = {0};
    sd.nextInChain = &sel.chain;
    RLWG.surface = wgpuInstanceCreateSurface(RLWG.instance, &sd);

    RLWGAdapterReq ar = {0};
    WGPURequestAdapterOptions opts = {0};
    opts.compatibleSurface = RLWG.surface;
    WGPURequestAdapterCallbackInfo aci = {0};
    aci.mode = WGPUCallbackMode_AllowSpontaneous;
    aci.callback = rlwgOnAdapter;
    aci.userdata1 = &ar;
    wgpuInstanceRequestAdapter(RLWG.instance, &opts, aci);
    while (!ar.done) emscripten_sleep(1);
    if (!ar.adapter) { TRACELOG(RL_LOG_FATAL, "RLWG: no WebGPU adapter"); return; }
    RLWG.adapter = ar.adapter;

    RLWGDeviceReq dr = {0};
    WGPURequestDeviceCallbackInfo dci = {0};
    dci.mode = WGPUCallbackMode_AllowSpontaneous;
    dci.callback = rlwgOnDevice;
    dci.userdata1 = &dr;
    wgpuAdapterRequestDevice(RLWG.adapter, NULL, dci);
    while (!dr.done) emscripten_sleep(1);
    if (!dr.device) { TRACELOG(RL_LOG_FATAL, "RLWG: no WebGPU device"); return; }
    RLWG.device = dr.device;
    RLWG.queue = wgpuDeviceGetQueue(RLWG.device);
}
#endif // __EMSCRIPTEN__

//----------------------------------------------------------------------------------
// rlglInit / rlglClose
//----------------------------------------------------------------------------------
void rlglInit(int width, int height)
{
    if (RLWG.inited) return; // platform layer already initialized (InitPlatform called us)

    RLWG.depthFormat = WGPUTextureFormat_Depth24Plus;
    RLWG.vSync = true;

    // Device/queue/surface may be provided by the platform layer up front
    // (rlwgAcquireDevice called from InitPlatform). Otherwise acquire them here.
#if defined(__EMSCRIPTEN__)
    if (!RLWG.device) rlwgAcquireDevice("#canvas");
#endif
    if (!RLWG.device) { TRACELOG(RL_LOG_FATAL, "RLWG: no WebGPU device available at rlglInit"); return; }
    if (!RLWG.queue) RLWG.queue = wgpuDeviceGetQueue(RLWG.device);

    // Query the preferred surface format so the pipeline target and swapchain agree.
    RLWG.surfaceFormat = WGPUTextureFormat_BGRA8Unorm; // safe default
#if defined(__EMSCRIPTEN__)
    if (RLWG.surface && RLWG.adapter)
    {
        WGPUSurfaceCapabilities caps = {0};
        if (wgpuSurfaceGetCapabilities(RLWG.surface, RLWG.adapter, &caps) == WGPUStatus_Success && caps.formatCount > 0)
            RLWG.surfaceFormat = caps.formats[0];
        wgpuSurfaceCapabilitiesFreeMembers(caps);
    }
#endif

    RLWG.State.framebufferWidth = width;
    RLWG.State.framebufferHeight = height;
    if (RLWG.surface) rlwgResize(width, height);
    else rlwgEnsureDepth(width, height);

    // Matrix state
    RLWG.State.modelview = rlwgMatrixIdentity();
    RLWG.State.projection = rlwgMatrixIdentity();
    RLWG.State.transform = rlwgMatrixIdentity();
    RLWG.State.currentMatrix = &RLWG.State.modelview;
    RLWG.State.currentMatrixMode = RL_MODELVIEW;

    // Default render state
    RLWG.State.currentBlendMode = RL_BLEND_ALPHA;
    RLWG.State.depthTestEnabled = false;
    RLWG.State.depthWriteEnabled = true;
    RLWG.State.cullEnabled = false;
    RLWG.State.colorMaskR = RLWG.State.colorMaskG = RLWG.State.colorMaskB = RLWG.State.colorMaskA = true;
    RLWG.State.colorr = RLWG.State.colorg = RLWG.State.colorb = RLWG.State.colora = 255;
    RLWG.clearColor = (WGPUColor){ 0, 0, 0, 1 };

    // Capabilities
    RLWG.ExtSupported.vao = true;
    RLWG.ExtSupported.instancing = true;
    RLWG.ExtSupported.texNPOT = true;
    RLWG.ExtSupported.texFloat32 = true;
    RLWG.ExtSupported.texFloat16 = true;
    RLWG.ExtSupported.computeShader = true;
    RLWG.ExtSupported.ssbo = true;
    RLWG.ExtSupported.maxDepthBits = 24;
    RLWG.ExtSupported.maxAnisotropyLevel = 16.0f;

    // Default 1x1 white texture
    unsigned char white[4] = { 255, 255, 255, 255 };
    RLWG.State.defaultTextureId = rlLoadTexture(white, 1, 1, RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);

    // Default shader
    RLWG.State.defaultShaderId = rlwgLoadDefaultShader();
    RLWG.State.currentShaderId = RLWG.State.defaultShaderId;

    static int defaultLocs[RL_MAX_SHADER_LOCATIONS];
    for (int i = 0; i < RL_MAX_SHADER_LOCATIONS; i++) defaultLocs[i] = -1;
    defaultLocs[RL_SHADER_LOC_VERTEX_POSITION] = 0;
    defaultLocs[RL_SHADER_LOC_VERTEX_TEXCOORD01] = 1;
    defaultLocs[RL_SHADER_LOC_VERTEX_NORMAL] = 2;
    defaultLocs[RL_SHADER_LOC_VERTEX_COLOR] = 3;
    defaultLocs[RL_SHADER_LOC_MATRIX_MVP] = 0;            // vertex UBO byte offset
    defaultLocs[RL_SHADER_LOC_MATRIX_VIEW] = 64;
    defaultLocs[RL_SHADER_LOC_MATRIX_PROJECTION] = 128;
    defaultLocs[RL_SHADER_LOC_MATRIX_MODEL] = 192;
    defaultLocs[RL_SHADER_LOC_MATRIX_NORMAL] = 256;
    defaultLocs[RL_SHADER_LOC_COLOR_DIFFUSE] = 0;         // fragment UBO byte offset
    defaultLocs[RL_SHADER_LOC_COLOR_SPECULAR] = 16;
    defaultLocs[RL_SHADER_LOC_VECTOR_VIEW] = 32;
    defaultLocs[RL_SHADER_LOC_MAP_ALBEDO] = 2;            // texture binding slot
    RLWG.State.defaultShaderLocs = defaultLocs;
    RLWG.State.currentShaderLocs = defaultLocs;

    RLWG.inited = true;
    TRACELOG(RL_LOG_INFO, "RLWG: WebGPU backend initialized (%dx%d)", width, height);
}

void rlglClose(void)
{
    rlUnloadTexture(RLWG.State.defaultTextureId);
    if (RLWG.depthTextureView) wgpuTextureViewRelease(RLWG.depthTextureView);
    if (RLWG.depthTexture) wgpuTextureRelease(RLWG.depthTexture);
    RL_FREE(RLWG.frameVertices);
    RL_FREE(RLWG.framePrimitives);
    RL_FREE(RLWG.textures);
    RL_FREE(RLWG.shaders);
    RL_FREE(RLWG.framebuffers);
    RL_FREE(RLWG.vertexBuffers);
    RL_FREE(RLWG.pipelineCache);
    for (int i = 0; i < RLWG.frameUBOCount; i++) wgpuBufferRelease(RLWG.frameUBOs[i]);
    RL_FREE(RLWG.frameUBOs);
    RLWG.inited = false;
}

void rlLoadExtensions(void *loader) { (void)loader; }
int rlGetVersion(void) { return RL_OPENGL_33; }

//----------------------------------------------------------------------------------
// Default + custom shaders
//----------------------------------------------------------------------------------
static WGPUShaderModule rlwgCreateModule(const char *wgsl)
{
    WGPUShaderSourceWGSL src = {0};
    src.chain.sType = WGPUSType_ShaderSourceWGSL;
    src.code = rlwgStr(wgsl);
    WGPUShaderModuleDescriptor desc = {0};
    desc.nextInChain = &src.chain;
    return wgpuDeviceCreateShaderModule(RLWG.device, &desc);
}

static void rlwgBuildShaderLayout(RLWGShaderRecord *sh)
{
    WGPUBindGroupLayoutEntry e[4] = {0};
    e[0].binding = 0; e[0].visibility = WGPUShaderStage_Vertex;   e[0].buffer.type = WGPUBufferBindingType_Uniform;
    e[1].binding = 1; e[1].visibility = WGPUShaderStage_Fragment; e[1].buffer.type = WGPUBufferBindingType_Uniform;
    e[2].binding = 2; e[2].visibility = WGPUShaderStage_Fragment; e[2].texture.sampleType = WGPUTextureSampleType_Float; e[2].texture.viewDimension = WGPUTextureViewDimension_2D;
    e[3].binding = 3; e[3].visibility = WGPUShaderStage_Fragment; e[3].sampler.type = WGPUSamplerBindingType_Filtering;

    WGPUBindGroupLayoutDescriptor bgld = {0};
    bgld.entryCount = 4; bgld.entries = e;
    sh->bindGroupLayout = wgpuDeviceCreateBindGroupLayout(RLWG.device, &bgld);

    WGPUPipelineLayoutDescriptor pld = {0};
    pld.bindGroupLayoutCount = 1; pld.bindGroupLayouts = &sh->bindGroupLayout;
    sh->pipelineLayout = wgpuDeviceCreatePipelineLayout(RLWG.device, &pld);

    // NOTE: uniform buffers are allocated per-flush (see rlwgUploadUniforms), not per
    // shader - a shared UBO cannot be reused across flushes within a frame.

    for (int i = 0; i < RL_MAX_SHADER_LOCATIONS; i++) sh->attribLocs[i] = -1;
    sh->attribLocs[RL_SHADER_LOC_VERTEX_POSITION] = 0;
    sh->attribLocs[RL_SHADER_LOC_VERTEX_TEXCOORD01] = 1;
    sh->attribLocs[RL_SHADER_LOC_VERTEX_NORMAL] = 2;
    sh->attribLocs[RL_SHADER_LOC_VERTEX_COLOR] = 3;
}

static unsigned int rlwgLoadDefaultShader(void)
{
    RLWGShaderRecord *sh = rlwgAllocShader();
    sh->id = ++RLWG.nextShaderId;
    sh->isDefault = true;
    sh->module = rlwgCreateModule(rlwgDefaultShaderWGSL);
    strcpy(sh->vsEntry, "vs_main");
    strcpy(sh->fsEntry, "fs_main");
    rlwgBuildShaderLayout(sh);
    return sh->id;
}

// Custom shaders: rlwg accepts a single WGSL source string holding both vs_main and
// fs_main. raylib passes vsCode/fsCode separately; for WGSL we concatenate (the
// caller is expected to supply WGSL, GL 3.3 GLSL must be offline-translated). If
// only one side is given, both stages must live in that one source.
unsigned int rlLoadShaderProgram(const char *vsCode, const char *fsCode)
{
    const char *combined = NULL;
    char *scratch = NULL;
    if (vsCode && fsCode && vsCode != fsCode && strstr(vsCode, "fs_main") == NULL)
    {
        size_t n = strlen(vsCode) + strlen(fsCode) + 2;
        scratch = (char *)RL_MALLOC(n);
        snprintf(scratch, n, "%s\n%s", vsCode, fsCode);
        combined = scratch;
    }
    else combined = vsCode ? vsCode : fsCode;

    if (!combined) return RLWG.State.defaultShaderId;

    RLWGShaderRecord *sh = rlwgAllocShader();
    sh->id = ++RLWG.nextShaderId;
    sh->module = rlwgCreateModule(combined);
    strcpy(sh->vsEntry, "vs_main");
    strcpy(sh->fsEntry, "fs_main");
    rlwgBuildShaderLayout(sh);
    if (scratch) RL_FREE(scratch);
    if (!sh->module) { sh->used = false; return RLWG.State.defaultShaderId; }
    return sh->id;
}

unsigned int rlLoadShader(const char *code, int type) { (void)code; (void)type; return 0; }
unsigned int rlLoadShaderProgramEx(unsigned int vId, unsigned int fId) { (void)vId; (void)fId; return 0; }
unsigned int rlLoadShaderProgramCompute(unsigned int csId) { (void)csId; return 0; }

void rlUnloadShaderProgram(unsigned int id)
{
    RLWGShaderRecord *sh = rlwgGetShader(id);
    if (!sh || sh->isDefault) return;
    if (sh->pipelineLayout) wgpuPipelineLayoutRelease(sh->pipelineLayout);
    if (sh->bindGroupLayout) wgpuBindGroupLayoutRelease(sh->bindGroupLayout);
    if (sh->module) wgpuShaderModuleRelease(sh->module);
    sh->used = false;
}

// The default-shader uniform locations are byte offsets into the UBOs; a custom
// shader keeps the same convention (matrices in the vertex UBO, colors in the frag
// UBO). For 1:1 GL behavior these names resolve to the canonical offsets.
int rlGetLocationUniform(unsigned int shaderId, const char *uniformName)
{
    (void)shaderId;
    if (!uniformName) return -1;
    if (!strcmp(uniformName, "mvp")) return 0;
    if (!strcmp(uniformName, "matView")) return 64;
    if (!strcmp(uniformName, "matProjection")) return 128;
    if (!strcmp(uniformName, "matModel")) return 192;
    if (!strcmp(uniformName, "matNormal")) return 256;
    if (!strcmp(uniformName, "colDiffuse")) return 0x10000 + 0;
    if (!strcmp(uniformName, "colSpecular")) return 0x10000 + 16;
    return -1;
}

int rlGetLocationAttrib(unsigned int shaderId, const char *attribName)
{
    (void)shaderId;
    if (!attribName) return -1;
    if (strstr(attribName, "Position")) return 0;
    if (strstr(attribName, "TexCoord2")) return 5;
    if (strstr(attribName, "TexCoord")) return 1;
    if (strstr(attribName, "Normal")) return 2;
    if (strstr(attribName, "Color")) return 3;
    if (strstr(attribName, "Tangent")) return 4;
    return -1;
}

// rlSetUniform writes into the CPU mirror; locations < 0x10000 target the vertex
// UBO, >= 0x10000 target the fragment UBO (offset = loc & 0xFFFF). Uploaded at flush.
static RLWGShaderUniformBlock RLWG_Uniforms;
static bool RLWG_UniformsInit = false;

void rlSetUniform(int locIndex, const void *value, int uniformType, int count)
{
    if (locIndex < 0 || !value) return;
    if (!RLWG_UniformsInit) { memset(&RLWG_Uniforms, 0, sizeof(RLWG_Uniforms)); RLWG_Uniforms.f.colDiffuse = (Vector4){1,1,1,1}; RLWG_UniformsInit = true; }

    int sizePerElem = 4;
    switch (uniformType)
    {
        case RL_SHADER_UNIFORM_VEC2: case RL_SHADER_UNIFORM_IVEC2: sizePerElem = 8; break;
        case RL_SHADER_UNIFORM_VEC3: case RL_SHADER_UNIFORM_IVEC3: sizePerElem = 12; break;
        case RL_SHADER_UNIFORM_VEC4: case RL_SHADER_UNIFORM_IVEC4: sizePerElem = 16; break;
        default: sizePerElem = 4; break;
    }
    int total = sizePerElem*(count > 0 ? count : 1);
    unsigned char *base;
    int offset;
    if (locIndex >= 0x10000) { base = (unsigned char *)&RLWG_Uniforms.f; offset = locIndex - 0x10000; }
    else { base = (unsigned char *)&RLWG_Uniforms.v; offset = locIndex; }
    if (offset + total <= (int)((locIndex >= 0x10000) ? sizeof(RLWGFragUBO) : sizeof(RLWGVertexUBO)))
        memcpy(base + offset, value, total);
}

void rlSetUniformMatrix(int locIndex, Matrix mat)
{
    if (locIndex < 0) return;
    if (!RLWG_UniformsInit) { memset(&RLWG_Uniforms, 0, sizeof(RLWG_Uniforms)); RLWG_Uniforms.f.colDiffuse = (Vector4){1,1,1,1}; RLWG_UniformsInit = true; }
    if (locIndex < (int)sizeof(RLWGVertexUBO)) memcpy(((unsigned char *)&RLWG_Uniforms.v) + locIndex, &mat, sizeof(Matrix));
}

void rlSetUniformMatrices(int locIndex, const Matrix *mat, int count)
{
    if (locIndex < 0 || !mat) return;
    for (int i = 0; i < count; i++) rlSetUniformMatrix(locIndex + i*(int)sizeof(Matrix), mat[i]);
}

void rlSetUniformSampler(int locIndex, unsigned int textureId)
{
    (void)locIndex;
    RLWG.State.activeTextureId[0] = textureId;
}

void rlSetShader(unsigned int id, int *locs)
{
    if (RLWG.State.currentShaderId != id) rlwgFlush();
    RLWG.State.currentShaderId = id ? id : RLWG.State.defaultShaderId;
    RLWG.State.currentShaderLocs = locs;
}

void rlEnableShader(unsigned int id) { if (RLWG.State.currentShaderId != id) rlwgFlush(); RLWG.State.currentShaderId = id ? id : RLWG.State.defaultShaderId; }
void rlDisableShader(void) { rlwgFlush(); RLWG.State.currentShaderId = RLWG.State.defaultShaderId; RLWG.State.currentShaderLocs = RLWG.State.defaultShaderLocs; }

unsigned int rlGetShaderIdDefault(void) { return RLWG.State.defaultShaderId; }
int *rlGetShaderLocsDefault(void) { return RLWG.State.defaultShaderLocs; }

//----------------------------------------------------------------------------------
// Pipeline cache
//----------------------------------------------------------------------------------
static uint64_t rlwgPipelineKey(RLWGShaderRecord *sh, int topology)
{
    uint64_t k = 0;
    k |= (uint64_t)(sh->id & 0xFFFF);
    k |= (uint64_t)(RLWG.State.currentBlendMode & 0xF) << 16;
    k |= (uint64_t)(RLWG.State.depthTestEnabled ? 1 : 0) << 20;
    k |= (uint64_t)(RLWG.State.depthWriteEnabled ? 1 : 0) << 21;
    k |= (uint64_t)(RLWG.State.cullEnabled ? 1 : 0) << 22;
    k |= (uint64_t)(topology & 0x3) << 23;
    k |= (uint64_t)(RLWG.activeFramebufferId & 0xFF) << 25;
    return k;
}

static WGPURenderPipeline rlwgGetPipeline(RLWGShaderRecord *sh, int topology)
{
    uint64_t key = rlwgPipelineKey(sh, topology);
    for (int i = 0; i < RLWG.pipelineCacheCount; i++)
        if (RLWG.pipelineCache[i].key == key) return RLWG.pipelineCache[i].pipeline;

    WGPUVertexAttribute attrs[4] = {0};
    attrs[0].format = WGPUVertexFormat_Float32x3; attrs[0].offset = 0;  attrs[0].shaderLocation = 0;
    attrs[1].format = WGPUVertexFormat_Float32x2; attrs[1].offset = 12; attrs[1].shaderLocation = 1;
    attrs[2].format = WGPUVertexFormat_Float32x3; attrs[2].offset = 20; attrs[2].shaderLocation = 2;
    attrs[3].format = WGPUVertexFormat_Unorm8x4;  attrs[3].offset = 32; attrs[3].shaderLocation = 3;

    WGPUVertexBufferLayout vbl = {0};
    vbl.arrayStride = sizeof(RLWGBatchVertex);
    vbl.stepMode = WGPUVertexStepMode_Vertex;
    vbl.attributeCount = 4; vbl.attributes = attrs;

    WGPURenderPipelineDescriptor pd = {0};
    pd.layout = sh->pipelineLayout;
    pd.vertex.module = sh->module;
    pd.vertex.entryPoint = rlwgStr(sh->vsEntry);
    pd.vertex.bufferCount = 1; pd.vertex.buffers = &vbl;

    WGPUBlendState blend = rlwgBlendStateForMode(RLWG.State.currentBlendMode);
    WGPUColorTargetState target = {0};
    target.format = (RLWG.activeFramebufferId != 0) ? WGPUTextureFormat_RGBA8Unorm : RLWG.surfaceFormat;
    target.blend = &blend;
    target.writeMask = WGPUColorWriteMask_All;

    WGPUFragmentState fs = {0};
    fs.module = sh->module;
    fs.entryPoint = rlwgStr(sh->fsEntry);
    fs.targetCount = 1; fs.targets = &target;
    pd.fragment = &fs;

    pd.primitive.topology = (topology == RL_LINES) ? WGPUPrimitiveTopology_LineList : WGPUPrimitiveTopology_TriangleList;
    pd.primitive.frontFace = WGPUFrontFace_CCW;
    pd.primitive.cullMode = RLWG.State.cullEnabled ? WGPUCullMode_Back : WGPUCullMode_None;

    WGPUDepthStencilState ds = {0};
    ds.format = RLWG.depthFormat;
    ds.depthCompare = RLWG.State.depthTestEnabled ? WGPUCompareFunction_LessEqual : WGPUCompareFunction_Always;
    ds.depthWriteEnabled = RLWG.State.depthWriteEnabled ? WGPUOptionalBool_True : WGPUOptionalBool_False;
    ds.stencilFront.compare = WGPUCompareFunction_Always;
    ds.stencilFront.failOp = WGPUStencilOperation_Keep;
    ds.stencilFront.depthFailOp = WGPUStencilOperation_Keep;
    ds.stencilFront.passOp = WGPUStencilOperation_Keep;
    ds.stencilBack.compare = WGPUCompareFunction_Always;
    ds.stencilBack.failOp = WGPUStencilOperation_Keep;
    ds.stencilBack.depthFailOp = WGPUStencilOperation_Keep;
    ds.stencilBack.passOp = WGPUStencilOperation_Keep;
    pd.depthStencil = &ds;

    pd.multisample.count = 1;
    pd.multisample.mask = 0xFFFFFFFF;

    WGPURenderPipeline pipe = wgpuDeviceCreateRenderPipeline(RLWG.device, &pd);

    if (RLWG.pipelineCacheCount >= RLWG.pipelineCacheCapacity)
    {
        int nc = RLWG.pipelineCacheCapacity > 0 ? RLWG.pipelineCacheCapacity*2 : 32;
        RLWG.pipelineCache = (struct RLWGPipelineEntry *)RL_REALLOC(RLWG.pipelineCache, (size_t)nc*sizeof(*RLWG.pipelineCache));
        RLWG.pipelineCacheCapacity = nc;
    }
    RLWG.pipelineCache[RLWG.pipelineCacheCount].key = key;
    RLWG.pipelineCache[RLWG.pipelineCacheCount].pipeline = pipe;
    RLWG.pipelineCacheCount++;
    return pipe;
}

//----------------------------------------------------------------------------------
// Frame cycle
//----------------------------------------------------------------------------------
void rlBeginFrame(void)
{
    if (!RLWG.inited || !RLWG.surface) return;

    WGPUSurfaceTexture st = {0};
    wgpuSurfaceGetCurrentTexture(RLWG.surface, &st);
    // Accept both SuccessOptimal (0) and SuccessSuboptimal (1); anything else
    // means the surface is lost or unavailable — skip this frame.
    if (st.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
        st.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal)
    {
        if (st.texture) wgpuTextureRelease(st.texture);
        TRACELOG(RL_LOG_WARNING, "RLWG: surface texture unavailable (status %d) - reconfiguring", (int)st.status);
        rlwgResize(RLWG.State.framebufferWidth, RLWG.State.framebufferHeight);
        return;
    }
    WGPUTextureViewDescriptor vd = {0};
    vd.format = RLWG.surfaceFormat;
    vd.dimension = WGPUTextureViewDimension_2D;
    vd.mipLevelCount = 1; vd.arrayLayerCount = 1;
    RLWG.currentColorView = wgpuTextureCreateView(st.texture, &vd);
    rlwgEnsureDepth(RLWG.State.framebufferWidth, RLWG.State.framebufferHeight);
    RLWG.currentDepthView = RLWG.depthTextureView;

    WGPUCommandEncoderDescriptor ed = {0};
    RLWG.encoder = wgpuDeviceCreateCommandEncoder(RLWG.device, &ed);
    RLWG.mainPassCleared = false;
    RLWG.passOpen = false;
    RLWG.frameVertexCount = 0;
    RLWG.framePrimitiveCount = 0;
}

static void rlwgBeginPassIfNeeded(void)
{
    if (RLWG.passOpen) return;
    if (!RLWG.encoder || !RLWG.currentColorView) return;

    WGPURenderPassColorAttachment ca = {0};
    ca.view = RLWG.currentColorView;
    ca.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    ca.loadOp = RLWG.mainPassCleared ? WGPULoadOp_Load : WGPULoadOp_Clear;
    ca.storeOp = WGPUStoreOp_Store;
    ca.clearValue = RLWG.clearColor;

    WGPURenderPassDepthStencilAttachment da = {0};
    da.view = RLWG.currentDepthView;
    da.depthLoadOp = RLWG.mainPassCleared ? WGPULoadOp_Load : WGPULoadOp_Clear;
    da.depthStoreOp = WGPUStoreOp_Store;
    da.depthClearValue = 1.0f;

    WGPURenderPassDescriptor rp = {0};
    rp.colorAttachmentCount = 1; rp.colorAttachments = &ca;
    rp.depthStencilAttachment = &da;
    RLWG.pass = wgpuCommandEncoderBeginRenderPass(RLWG.encoder, &rp);
    RLWG.passOpen = true;
    RLWG.mainPassCleared = true;
}

void rlEndFrame(void)
{
    if (!RLWG.inited || !RLWG.encoder) return;
    rlwgFlush();
    // Ensure the backbuffer is cleared even if nothing drew this frame.
    if (!RLWG.mainPassCleared) rlwgBeginPassIfNeeded();
    if (RLWG.passOpen) { wgpuRenderPassEncoderEnd(RLWG.pass); wgpuRenderPassEncoderRelease(RLWG.pass); RLWG.pass = NULL; RLWG.passOpen = false; }

    WGPUCommandBufferDescriptor cd = {0};
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(RLWG.encoder, &cd);
    wgpuQueueSubmit(RLWG.queue, 1, &cmd);
    wgpuCommandBufferRelease(cmd);
    wgpuCommandEncoderRelease(RLWG.encoder);
    RLWG.encoder = NULL;

#if !defined(__EMSCRIPTEN__)
    if (RLWG.surface) wgpuSurfacePresent(RLWG.surface);
#endif
    if (RLWG.currentColorView) { wgpuTextureViewRelease(RLWG.currentColorView); RLWG.currentColorView = NULL; }

    // The submitted command buffer no longer needs the per-flush UBOs; release them.
    for (int i = 0; i < RLWG.frameUBOCount; i++) wgpuBufferRelease(RLWG.frameUBOs[i]);
    RLWG.frameUBOCount = 0;
}

//----------------------------------------------------------------------------------
// Batch flush - expands QUADS to triangles, uploads, draws per primitive
//----------------------------------------------------------------------------------
// Track a transient buffer for release after this frame's command buffer is submitted.
static void rlwgTrackFrameUBO(WGPUBuffer b)
{
    if (RLWG.frameUBOCount >= RLWG.frameUBOCapacity)
    {
        int nc = RLWG.frameUBOCapacity > 0 ? RLWG.frameUBOCapacity*2 : 16;
        RLWG.frameUBOs = (WGPUBuffer *)RL_REALLOC(RLWG.frameUBOs, (size_t)nc*sizeof(WGPUBuffer));
        RLWG.frameUBOCapacity = nc;
    }
    RLWG.frameUBOs[RLWG.frameUBOCount++] = b;
}

// Create + fill a fresh vertex+fragment UBO pair for this flush (see frameUBOs note in
// rlwgData). Returns them through outV/outF; both are tracked for end-of-frame release.
static void rlwgUploadUniforms(WGPUBuffer *outV, WGPUBuffer *outF)
{
    if (!RLWG_UniformsInit) { memset(&RLWG_Uniforms, 0, sizeof(RLWG_Uniforms)); RLWG_Uniforms.f.colDiffuse = (Vector4){1,1,1,1}; RLWG_UniformsInit = true; }
    // mvp = modelview * projection (genuine rlgl convention - same as rlDrawRenderBatch).
    RLWG_Uniforms.v.mvp = rlwgMatrixMultiply(RLWG.State.modelview, RLWG.State.projection);
    RLWG_Uniforms.v.view = RLWG.State.modelview;
    RLWG_Uniforms.v.projection = RLWG.State.projection;
    RLWG_Uniforms.f.resolution = (Vector2){ (float)RLWG.State.framebufferWidth, (float)RLWG.State.framebufferHeight };

    WGPUBufferDescriptor vd = {0};
    vd.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst; vd.size = sizeof(RLWGVertexUBO);
    WGPUBuffer vUBO = wgpuDeviceCreateBuffer(RLWG.device, &vd);
    WGPUBufferDescriptor fd = {0};
    fd.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst; fd.size = sizeof(RLWGFragUBO);
    WGPUBuffer fUBO = wgpuDeviceCreateBuffer(RLWG.device, &fd);

    // IMPORTANT: raylib's Matrix struct memory order is m0,m4,m8,m12,... (declaration
    // order), which is the transpose of WGSL's column-major mat4x4. Convert each matrix
    // to column-major (m0,m1,m2,m3 = column 0) exactly like raylib's MatrixToFloat does
    // before uploading. The fragment UBO holds only vectors, so it uploads as-is.
    float vbuf[80];
    rlwgMatrixToFloats(RLWG_Uniforms.v.mvp,        &vbuf[0]);
    rlwgMatrixToFloats(RLWG_Uniforms.v.view,       &vbuf[16]);
    rlwgMatrixToFloats(RLWG_Uniforms.v.projection, &vbuf[32]);
    rlwgMatrixToFloats(RLWG_Uniforms.v.model,      &vbuf[48]);
    rlwgMatrixToFloats(RLWG_Uniforms.v.normalMat,  &vbuf[64]);
    wgpuQueueWriteBuffer(RLWG.queue, vUBO, 0, vbuf, sizeof(vbuf));
    wgpuQueueWriteBuffer(RLWG.queue, fUBO, 0, &RLWG_Uniforms.f, sizeof(RLWGFragUBO));

    rlwgTrackFrameUBO(vUBO);
    rlwgTrackFrameUBO(fUBO);
    *outV = vUBO; *outF = fUBO;
}

static WGPUBindGroup rlwgMakeBindGroup(RLWGShaderRecord *sh, WGPUBuffer vUBO, WGPUBuffer fUBO, unsigned int textureId)
{
    RLWGTextureRecord *tex = rlwgGetTexture(textureId);
    if (!tex) tex = rlwgGetTexture(RLWG.State.defaultTextureId);

    WGPUBindGroupEntry e[4] = {0};
    e[0].binding = 0; e[0].buffer = vUBO; e[0].size = sizeof(RLWGVertexUBO);
    e[1].binding = 1; e[1].buffer = fUBO; e[1].size = sizeof(RLWGFragUBO);
    e[2].binding = 2; e[2].textureView = tex ? tex->view : NULL;
    e[3].binding = 3; e[3].sampler = tex ? tex->sampler : NULL;

    WGPUBindGroupDescriptor d = {0};
    d.layout = sh->bindGroupLayout;
    d.entryCount = 4; d.entries = e;
    return wgpuDeviceCreateBindGroup(RLWG.device, &d);
}

static void rlwgFlush(void)
{
    if (RLWG.framePrimitiveCount == 0) { RLWG.frameVertexCount = 0; return; }
    RLWGShaderRecord *sh = rlwgGetShader(RLWG.State.currentShaderId);
    if (!sh) sh = rlwgGetShader(RLWG.State.defaultShaderId);
    if (!sh) { RLWG.frameVertexCount = 0; RLWG.framePrimitiveCount = 0; return; }

    // Build expanded (quad->triangle) vertex array.
    int maxExpanded = 0;
    for (int i = 0; i < RLWG.framePrimitiveCount; i++)
    {
        int vc = RLWG.framePrimitives[i].vertexCount;
        maxExpanded += (RLWG.framePrimitives[i].mode == RL_QUADS) ? (vc/4)*6 : vc;
    }
    if (maxExpanded == 0) { RLWG.frameVertexCount = 0; RLWG.framePrimitiveCount = 0; return; }

    // NOTE: drawList/bindGroups are heap-allocated (sized to the primitive count).
    // They must NOT live on the stack: Emscripten's default stack is only 64 KB and
    // a per-primitive array would overflow it and silently corrupt memory.
    RLWGBatchVertex *expanded = (RLWGBatchVertex *)RL_MALLOC((size_t)maxExpanded*sizeof(RLWGBatchVertex));
    struct RLWGDrawItem { int start, count, topology; unsigned int textureId; };
    struct RLWGDrawItem *drawList = (struct RLWGDrawItem *)RL_MALLOC((size_t)RLWG.framePrimitiveCount*sizeof(struct RLWGDrawItem));
    int drawCount = 0;
    int writePos = 0;

    for (int i = 0; i < RLWG.framePrimitiveCount; i++)
    {
        int mode = RLWG.framePrimitives[i].mode;
        int sv = RLWG.framePrimitives[i].startVertex;
        int vc = RLWG.framePrimitives[i].vertexCount;
        int start = writePos;
        if (mode == RL_QUADS)
        {
            for (int q = 0; q + 3 < vc; q += 4)
            {
                RLWGBatchVertex *v = &RLWG.frameVertices[sv + q];
                expanded[writePos++] = v[0]; expanded[writePos++] = v[1]; expanded[writePos++] = v[2];
                expanded[writePos++] = v[0]; expanded[writePos++] = v[2]; expanded[writePos++] = v[3];
            }
        }
        else
        {
            for (int k = 0; k < vc; k++) expanded[writePos++] = RLWG.frameVertices[sv + k];
        }
        drawList[drawCount].start = start;
        drawList[drawCount].count = writePos - start;
        drawList[drawCount].topology = (mode == RL_LINES) ? RL_LINES : RL_TRIANGLES;
        drawList[drawCount].textureId = RLWG.framePrimitives[i].textureId;
        drawCount++;
    }

    // Upload to a per-flush GPU vertex buffer. Like the UBOs, this CANNOT be a single
    // shared/growing buffer reused across flushes: wgpuQueueWriteBuffer writes all apply
    // before the command buffer executes, so every draw would read the LAST flush's
    // vertices. Each flush gets its own buffer, released after submit in rlEndFrame.
    size_t bytes = (size_t)writePos*sizeof(RLWGBatchVertex);
    WGPUBufferDescriptor bd = {0};
    bd.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
    bd.size = bytes;
    WGPUBuffer vbuf = wgpuDeviceCreateBuffer(RLWG.device, &bd);
    wgpuQueueWriteBuffer(RLWG.queue, vbuf, 0, expanded, bytes);
    rlwgTrackFrameUBO(vbuf);

    WGPUBuffer flushVUBO = NULL, flushFUBO = NULL;
    rlwgUploadUniforms(&flushVUBO, &flushFUBO);

    rlwgBeginPassIfNeeded();

    WGPUBindGroup *bindGroups = (WGPUBindGroup *)RL_MALLOC((size_t)(drawCount > 0 ? drawCount : 1)*sizeof(WGPUBindGroup));
    int bgCount = 0;
    if (RLWG.passOpen)
    {
        wgpuRenderPassEncoderSetVertexBuffer(RLWG.pass, 0, vbuf, 0, bytes);
        for (int i = 0; i < drawCount; i++)
        {
            WGPURenderPipeline pipe = rlwgGetPipeline(sh, drawList[i].topology);
            wgpuRenderPassEncoderSetPipeline(RLWG.pass, pipe);
            WGPUBindGroup bg = rlwgMakeBindGroup(sh, flushVUBO, flushFUBO, drawList[i].textureId);
            bindGroups[bgCount++] = bg;
            wgpuRenderPassEncoderSetBindGroup(RLWG.pass, 0, bg, 0, NULL);
            wgpuRenderPassEncoderDraw(RLWG.pass, (uint32_t)drawList[i].count, 1, (uint32_t)drawList[i].start, 0);
        }
    }

    RL_FREE(expanded);
    for (int i = 0; i < bgCount; i++) wgpuBindGroupRelease(bindGroups[i]);
    RL_FREE(bindGroups);
    RL_FREE(drawList);

    RLWG.frameVertexCount = 0;
    RLWG.framePrimitiveCount = 0;
}

//----------------------------------------------------------------------------------
// Immediate-mode batch
//----------------------------------------------------------------------------------
void rlBegin(int mode)
{
    RLWG.State.currentDrawMode = mode;
    if (rlwgEnsureFramePrimitiveCapacity(RLWG.framePrimitiveCount + 1))
    {
        int p = RLWG.framePrimitiveCount++;
        RLWG.framePrimitives[p].mode = mode;
        RLWG.framePrimitives[p].startVertex = RLWG.frameVertexCount;
        RLWG.framePrimitives[p].vertexCount = 0;
        RLWG.framePrimitives[p].textureId = RLWG.State.currentTextureId ? RLWG.State.currentTextureId : RLWG.State.defaultTextureId;
        RLWG.framePrimitives[p].shaderId = RLWG.State.currentShaderId;
    }
}

void rlEnd(void) { /* primitive already finalized incrementally */ }

void rlVertex3f(float x, float y, float z)
{
    if (!rlwgEnsureFrameVertexCapacity(RLWG.frameVertexCount + 1)) return;
    Vector3 p = { x, y, z };
    if (RLWG.State.transformRequired)
    {
        Matrix m = RLWG.State.transform;
        p.x = m.m0*x + m.m4*y + m.m8*z + m.m12;
        p.y = m.m1*x + m.m5*y + m.m9*z + m.m13;
        p.z = m.m2*x + m.m6*y + m.m10*z + m.m14;
    }
    RLWGBatchVertex *v = &RLWG.frameVertices[RLWG.frameVertexCount++];
    v->position[0] = p.x; v->position[1] = p.y; v->position[2] = p.z;
    v->texcoord[0] = RLWG.State.texcoordx; v->texcoord[1] = RLWG.State.texcoordy;
    v->normal[0] = RLWG.State.normalx; v->normal[1] = RLWG.State.normaly; v->normal[2] = RLWG.State.normalz;
    v->color[0] = RLWG.State.colorr; v->color[1] = RLWG.State.colorg; v->color[2] = RLWG.State.colorb; v->color[3] = RLWG.State.colora;
    if (RLWG.framePrimitiveCount > 0) RLWG.framePrimitives[RLWG.framePrimitiveCount-1].vertexCount++;
}

void rlVertex2f(float x, float y) { rlVertex3f(x, y, 0.0f); }
void rlVertex2i(int x, int y) { rlVertex3f((float)x, (float)y, 0.0f); }
void rlTexCoord2f(float x, float y) { RLWG.State.texcoordx = x; RLWG.State.texcoordy = y; }
void rlNormal3f(float x, float y, float z) { RLWG.State.normalx = x; RLWG.State.normaly = y; RLWG.State.normalz = z; }
void rlColor4ub(unsigned char r, unsigned char g, unsigned char b, unsigned char a) { RLWG.State.colorr = r; RLWG.State.colorg = g; RLWG.State.colorb = b; RLWG.State.colora = a; }
void rlColor3f(float r, float g, float b) { rlColor4ub((unsigned char)(r*255), (unsigned char)(g*255), (unsigned char)(b*255), 255); }
void rlColor4f(float r, float g, float b, float a) { rlColor4ub((unsigned char)(r*255), (unsigned char)(g*255), (unsigned char)(b*255), (unsigned char)(a*255)); }

void rlSetTexture(unsigned int id)
{
    if (id == 0) { RLWG.State.currentTextureId = RLWG.State.defaultTextureId; return; }
    if (RLWG.State.currentTextureId != id)
    {
        // New texture starts a new primitive group on the next rlBegin; raylib calls
        // rlSetTexture between rlEnd/rlBegin, so just record it.
        RLWG.State.currentTextureId = id;
    }
}

void rlDrawRenderBatchActive(void) { rlwgFlush(); }
void rlDrawRenderBatch(rlRenderBatch *batch) { (void)batch; rlwgFlush(); }
void rlSetRenderBatchActive(rlRenderBatch *batch) { RLWG.currentBatch = batch; }
bool rlCheckRenderBatchLimit(int vCount) { (void)vCount; return false; } // batch grows dynamically

rlRenderBatch rlLoadRenderBatch(int numBuffers, int bufferElements)
{
    (void)numBuffers; (void)bufferElements;
    rlRenderBatch b = {0};
    return b;
}
void rlUnloadRenderBatch(rlRenderBatch batch) { (void)batch; }

#include "rlwg_impl2.h"

#endif // RLWG_IMPL_H
