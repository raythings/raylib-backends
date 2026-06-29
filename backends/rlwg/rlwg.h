/**********************************************************************************************
*
*   rlwg - WebGPU (browser / Emscripten) backend for raylib, drop-in for rlgl
*
*   rlwg re-implements the rlgl API that raylib's rcore.c drives, emulating OpenGL 3.3
*   semantics 1:1 on top of the WebGPU C API - the same contract rlvk (Vulkan) and rlmt
*   (Metal) honor. It targets the *browser* WebGPU runtime via Emscripten's emdawnwebgpu
*   port (WGSL-only; no SPIR-V).
*
*   DESIGN (how it maps 1:1 to OpenGL 3.3):
*     - Fixed vertex attribute locations: 0 position, 1 texcoord, 2 normal, 3 color,
*       4 tangent, 5 texcoord2 - identical to rlgl/rlvk/rlmt.
*     - The genuine rlgl draw path: raw model-space vertices are uploaded and the shader
*       transforms by the `mvp` uniform (NOT CPU pre-transform). So stock raylib GLSL
*       shaders, ported to WGSL, behave exactly as on desktop GL.
*     - Loose GL uniforms are packed into uniform buffers (WebGPU has no globals); the
*       C mirror RLWGShaderUniformBlock matches the default-shader byte layout so
*       rlSetUniform()/rlSetUniformMatrix() write by offset (same scheme as rlvk).
*     - The one unavoidable WebGPU deviation - clip-space z [-1,1] -> [0,1] - is baked
*       into the default vertex shader (see rlwg_default_wgsl.h), invisible to callers.
*
*   This header is pulled ONLY by the overlaid rcore.c translation unit (the CMake
*   rcore overlay replaces `#define RLGL_IMPLEMENTATION / #include "rlgl.h"` with an
*   include of this file). Other raylib modules keep including the stock rlgl.h for
*   declarations, so there is no multiple-definition conflict - same model as rlvk/rlmt.
*
**********************************************************************************************/

#ifndef RLGL_H
#define RLGL_H

#include <webgpu/webgpu.h>
#if defined(__EMSCRIPTEN__)
    #include <emscripten/emscripten.h>
    #include <emscripten/html5.h>
#endif

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "rlwg_default_wgsl.h"

#define RLGL_VERSION  "6.0"

#if defined(_WIN32)
    #if defined(BUILD_LIBTYPE_SHARED)
        #define RLAPI __declspec(dllexport)
    #elif defined(USE_LIBTYPE_SHARED)
        #define RLAPI __declspec(dllimport)
    #endif
#endif
#ifndef RLAPI
    #define RLAPI
#endif

#ifndef RL_DEFAULT_BATCH_BUFFER_ELEMENTS
    #define RL_DEFAULT_BATCH_BUFFER_ELEMENTS 8192
#endif
#ifndef RL_DEFAULT_BATCH_BUFFERS
    #define RL_DEFAULT_BATCH_BUFFERS 1
#endif
#ifndef RL_DEFAULT_BATCH_DRAWCALLS
    #define RL_DEFAULT_BATCH_DRAWCALLS 256
#endif
#ifndef RL_DEFAULT_BATCH_MAX_TEXTURE_UNITS
    #define RL_DEFAULT_BATCH_MAX_TEXTURE_UNITS 4
#endif
#ifndef RL_MAX_MATRIX_STACK_SIZE
    #define RL_MAX_MATRIX_STACK_SIZE 32
#endif
#ifndef RL_MAX_SHADER_LOCATIONS
    #define RL_MAX_SHADER_LOCATIONS 32
#endif
#ifndef RL_CULL_DISTANCE_NEAR
    #define RL_CULL_DISTANCE_NEAR 0.01
#endif
#ifndef RL_CULL_DISTANCE_FAR
    #define RL_CULL_DISTANCE_FAR 1000.0
#endif

#ifndef TRACELOG
    #define TRACELOG(level, ...) (void)0
    #define TRACELOGD(...) (void)0
#endif

#ifndef RL_MALLOC
    #define RL_MALLOC(sz)     malloc(sz)
#endif
#ifndef RL_CALLOC
    #define RL_CALLOC(n,sz)   calloc(n,sz)
#endif
#ifndef RL_REALLOC
    #define RL_REALLOC(p,sz)  realloc(p,sz)
#endif
#ifndef RL_FREE
    #define RL_FREE(p)        free(p)
#endif

//----------------------------------------------------------------------------------
// rlgl public defines / enums / types (mirrors stock rlgl.h - this header replaces it)
//----------------------------------------------------------------------------------

// Texture parameters (equivalent to OpenGL defines)
#define RL_TEXTURE_WRAP_S                       0x2802
#define RL_TEXTURE_WRAP_T                       0x2803
#define RL_TEXTURE_MAG_FILTER                   0x2800
#define RL_TEXTURE_MIN_FILTER                   0x2801
#define RL_TEXTURE_FILTER_NEAREST               0x2600
#define RL_TEXTURE_FILTER_LINEAR                0x2601
#define RL_TEXTURE_FILTER_MIP_NEAREST           0x2700
#define RL_TEXTURE_FILTER_NEAREST_MIP_LINEAR    0x2702
#define RL_TEXTURE_FILTER_LINEAR_MIP_NEAREST    0x2701
#define RL_TEXTURE_FILTER_MIP_LINEAR            0x2703
#define RL_TEXTURE_FILTER_ANISOTROPIC           0x3000
#define RL_TEXTURE_MIPMAP_BIAS_RATIO            0x4000
#define RL_TEXTURE_WRAP_REPEAT                  0x2901
#define RL_TEXTURE_WRAP_CLAMP                   0x812F
#define RL_TEXTURE_WRAP_MIRROR_REPEAT           0x8370
#define RL_TEXTURE_WRAP_MIRROR_CLAMP            0x8742

// Matrix modes (equivalent to OpenGL)
#define RL_MODELVIEW                            0x1700
#define RL_PROJECTION                           0x1701
#define RL_TEXTURE                              0x1702

// Primitive assembly draw modes
#define RL_LINES                                0x0001
#define RL_TRIANGLES                            0x0004
#define RL_QUADS                                0x0007

// GL equivalent data types
#define RL_UNSIGNED_BYTE                        0x1401
#define RL_FLOAT                                0x1406

// GL buffer usage hint
#define RL_STREAM_DRAW                          0x88E0
#define RL_STREAM_READ                          0x88E1
#define RL_STREAM_COPY                          0x88E2
#define RL_STATIC_DRAW                          0x88E4
#define RL_STATIC_READ                          0x88E5
#define RL_STATIC_COPY                          0x88E6
#define RL_DYNAMIC_DRAW                         0x88E8
#define RL_DYNAMIC_READ                         0x88E9
#define RL_DYNAMIC_COPY                         0x88EA

// GL blending factors / equations (subset used by raylib)
#define RL_ZERO                                 0
#define RL_ONE                                  1
#define RL_SRC_COLOR                            0x0300
#define RL_ONE_MINUS_SRC_COLOR                  0x0301
#define RL_SRC_ALPHA                            0x0302
#define RL_ONE_MINUS_SRC_ALPHA                  0x0303
#define RL_DST_ALPHA                            0x0304
#define RL_ONE_MINUS_DST_ALPHA                  0x0305
#define RL_DST_COLOR                            0x0306
#define RL_ONE_MINUS_DST_COLOR                  0x0307
#define RL_SRC_ALPHA_SATURATE                   0x0308
#define RL_FUNC_ADD                             0x8006
#define RL_FUNC_SUBTRACT                        0x800A
#define RL_FUNC_REVERSE_SUBTRACT                0x800B
#define RL_BLEND_COLOR                          0x8005

// Default shader vertex attribute locations
#ifndef RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION
    #define RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION  0
#endif
#ifndef RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD
    #define RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD  1
#endif
#ifndef RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL
    #define RL_DEFAULT_SHADER_ATTRIB_LOCATION_NORMAL    2
#endif
#ifndef RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR
    #define RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR     3
#endif
#ifndef RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT
    #define RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT   4
#endif
#ifndef RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD2
    #define RL_DEFAULT_SHADER_ATTRIB_LOCATION_TEXCOORD2 5
#endif
#ifndef RL_DEFAULT_SHADER_ATTRIB_LOCATION_INDICES
    #define RL_DEFAULT_SHADER_ATTRIB_LOCATION_INDICES   6
#endif
#ifndef RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEINDICES
    #define RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEINDICES 7
#endif
#ifndef RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEWEIGHTS
    #define RL_DEFAULT_SHADER_ATTRIB_LOCATION_BONEWEIGHTS 8
#endif
#ifndef RL_DEFAULT_SHADER_ATTRIB_LOCATION_INSTANCETRANSFORM
    #define RL_DEFAULT_SHADER_ATTRIB_LOCATION_INSTANCETRANSFORM 9
#endif

// Default shader attribute/uniform names (rcore binds default-shader locations by name)
#define RL_DEFAULT_SHADER_ATTRIB_NAME_POSITION          "vertexPosition"
#define RL_DEFAULT_SHADER_ATTRIB_NAME_TEXCOORD          "vertexTexCoord"
#define RL_DEFAULT_SHADER_ATTRIB_NAME_NORMAL            "vertexNormal"
#define RL_DEFAULT_SHADER_ATTRIB_NAME_COLOR             "vertexColor"
#define RL_DEFAULT_SHADER_ATTRIB_NAME_TANGENT           "vertexTangent"
#define RL_DEFAULT_SHADER_ATTRIB_NAME_TEXCOORD2         "vertexTexCoord2"
#define RL_DEFAULT_SHADER_ATTRIB_NAME_BONEINDICES       "vertexBoneIds"
#define RL_DEFAULT_SHADER_ATTRIB_NAME_BONEWEIGHTS       "vertexBoneWeights"
#define RL_DEFAULT_SHADER_ATTRIB_NAME_INSTANCETRANSFORM "instanceTransform"
#define RL_DEFAULT_SHADER_UNIFORM_NAME_MVP              "mvp"
#define RL_DEFAULT_SHADER_UNIFORM_NAME_VIEW             "matView"
#define RL_DEFAULT_SHADER_UNIFORM_NAME_PROJECTION       "matProjection"
#define RL_DEFAULT_SHADER_UNIFORM_NAME_MODEL            "matModel"
#define RL_DEFAULT_SHADER_UNIFORM_NAME_NORMAL           "matNormal"
#define RL_DEFAULT_SHADER_UNIFORM_NAME_COLOR            "colDiffuse"
#define RL_DEFAULT_SHADER_UNIFORM_NAME_BONEMATRICES     "boneMatrices"
#define RL_DEFAULT_SHADER_SAMPLER2D_NAME_TEXTURE0       "texture0"
#define RL_DEFAULT_SHADER_SAMPLER2D_NAME_TEXTURE1       "texture1"
#define RL_DEFAULT_SHADER_SAMPLER2D_NAME_TEXTURE2       "texture2"

// GL shader stage selectors (rlLoadShader type argument)
#define RL_VERTEX_SHADER                        0x8B31
#define RL_FRAGMENT_SHADER                      0x8B30
#define RL_COMPUTE_SHADER                       0x91B9

typedef enum {
    RL_LOG_ALL = 0, RL_LOG_TRACE, RL_LOG_DEBUG, RL_LOG_INFO,
    RL_LOG_WARNING, RL_LOG_ERROR, RL_LOG_FATAL, RL_LOG_NONE
} rlTraceLogLevel;

typedef enum {
    RL_OPENGL_11 = 1, RL_OPENGL_21, RL_OPENGL_33, RL_OPENGL_43,
    RL_OPENGL_ES_20, RL_OPENGL_ES_30
} rlGlVersion;

typedef enum {
    RL_ATTACHMENT_COLOR_CHANNEL0 = 0, RL_ATTACHMENT_COLOR_CHANNEL1, RL_ATTACHMENT_COLOR_CHANNEL2,
    RL_ATTACHMENT_COLOR_CHANNEL3, RL_ATTACHMENT_COLOR_CHANNEL4, RL_ATTACHMENT_COLOR_CHANNEL5,
    RL_ATTACHMENT_COLOR_CHANNEL6, RL_ATTACHMENT_COLOR_CHANNEL7,
    RL_ATTACHMENT_DEPTH = 100, RL_ATTACHMENT_STENCIL = 200
} rlFramebufferAttachType;

typedef enum {
    RL_ATTACHMENT_CUBEMAP_POSITIVE_X = 0, RL_ATTACHMENT_CUBEMAP_NEGATIVE_X,
    RL_ATTACHMENT_CUBEMAP_POSITIVE_Y, RL_ATTACHMENT_CUBEMAP_NEGATIVE_Y,
    RL_ATTACHMENT_CUBEMAP_POSITIVE_Z, RL_ATTACHMENT_CUBEMAP_NEGATIVE_Z,
    RL_ATTACHMENT_TEXTURE2D = 100, RL_ATTACHMENT_RENDERBUFFER = 200
} rlFramebufferAttachTextureType;

typedef enum {
    RL_BLEND_ALPHA = 0, RL_BLEND_ADDITIVE, RL_BLEND_MULTIPLIED, RL_BLEND_ADD_COLORS,
    RL_BLEND_SUBTRACT_COLORS, RL_BLEND_ALPHA_PREMULTIPLY, RL_BLEND_CUSTOM, RL_BLEND_CUSTOM_SEPARATE
} rlBlendMode;

typedef enum {
    RL_SHADER_LOC_VERTEX_POSITION = 0, RL_SHADER_LOC_VERTEX_TEXCOORD01, RL_SHADER_LOC_VERTEX_TEXCOORD02,
    RL_SHADER_LOC_VERTEX_NORMAL, RL_SHADER_LOC_VERTEX_TANGENT, RL_SHADER_LOC_VERTEX_COLOR,
    RL_SHADER_LOC_MATRIX_MVP, RL_SHADER_LOC_MATRIX_VIEW, RL_SHADER_LOC_MATRIX_PROJECTION,
    RL_SHADER_LOC_MATRIX_MODEL, RL_SHADER_LOC_MATRIX_NORMAL, RL_SHADER_LOC_VECTOR_VIEW,
    RL_SHADER_LOC_COLOR_DIFFUSE, RL_SHADER_LOC_COLOR_SPECULAR, RL_SHADER_LOC_COLOR_AMBIENT,
    RL_SHADER_LOC_MAP_ALBEDO, RL_SHADER_LOC_MAP_METALNESS, RL_SHADER_LOC_MAP_NORMAL,
    RL_SHADER_LOC_MAP_ROUGHNESS, RL_SHADER_LOC_MAP_OCCLUSION, RL_SHADER_LOC_MAP_EMISSION,
    RL_SHADER_LOC_MAP_HEIGHT, RL_SHADER_LOC_MAP_CUBEMAP, RL_SHADER_LOC_MAP_IRRADIANCE,
    RL_SHADER_LOC_MAP_PREFILTER, RL_SHADER_LOC_MAP_BRDF
} rlShaderLocationIndex;

typedef enum {
    RL_SHADER_UNIFORM_FLOAT = 0, RL_SHADER_UNIFORM_VEC2, RL_SHADER_UNIFORM_VEC3, RL_SHADER_UNIFORM_VEC4,
    RL_SHADER_UNIFORM_INT, RL_SHADER_UNIFORM_IVEC2, RL_SHADER_UNIFORM_IVEC3, RL_SHADER_UNIFORM_IVEC4,
    RL_SHADER_UNIFORM_UINT, RL_SHADER_UNIFORM_UIVEC2, RL_SHADER_UNIFORM_UIVEC3, RL_SHADER_UNIFORM_UIVEC4,
    RL_SHADER_UNIFORM_SAMPLER2D
} rlShaderUniformDataType;

typedef enum {
    RL_SHADER_ATTRIB_FLOAT = 0, RL_SHADER_ATTRIB_VEC2, RL_SHADER_ATTRIB_VEC3, RL_SHADER_ATTRIB_VEC4
} rlShaderAttributeDataType;

typedef enum {
    RL_PIXELFORMAT_UNCOMPRESSED_GRAYSCALE = 1, RL_PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA,
    RL_PIXELFORMAT_UNCOMPRESSED_R5G6B5, RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8,
    RL_PIXELFORMAT_UNCOMPRESSED_R5G5B5A1, RL_PIXELFORMAT_UNCOMPRESSED_R4G4B4A4,
    RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, RL_PIXELFORMAT_UNCOMPRESSED_R32,
    RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32, RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32A32,
    RL_PIXELFORMAT_UNCOMPRESSED_R16, RL_PIXELFORMAT_UNCOMPRESSED_R16G16B16,
    RL_PIXELFORMAT_UNCOMPRESSED_R16G16B16A16,
    RL_PIXELFORMAT_COMPRESSED_DXT1_RGB, RL_PIXELFORMAT_COMPRESSED_DXT1_RGBA,
    RL_PIXELFORMAT_COMPRESSED_DXT3_RGBA, RL_PIXELFORMAT_COMPRESSED_DXT5_RGBA,
    RL_PIXELFORMAT_COMPRESSED_ETC1_RGB, RL_PIXELFORMAT_COMPRESSED_ETC2_RGB,
    RL_PIXELFORMAT_COMPRESSED_ETC2_EAC_RGBA, RL_PIXELFORMAT_COMPRESSED_PVRT_RGB,
    RL_PIXELFORMAT_COMPRESSED_PVRT_RGBA, RL_PIXELFORMAT_COMPRESSED_ASTC_4x4_RGBA,
    RL_PIXELFORMAT_COMPRESSED_ASTC_8x8_RGBA
} rlPixelFormat;

typedef enum {
    RL_TEXTURE_FILTER_POINT = 0, RL_TEXTURE_FILTER_BILINEAR, RL_TEXTURE_FILTER_TRILINEAR,
    RL_TEXTURE_FILTER_ANISOTROPIC_4X, RL_TEXTURE_FILTER_ANISOTROPIC_8X, RL_TEXTURE_FILTER_ANISOTROPIC_16X
} rlTextureFilter;

// raylib math types (only defined if raylib.h has not already provided them)
#if !defined(RL_VECTOR2_TYPE)
typedef struct Vector2 { float x, y; } Vector2;
#define RL_VECTOR2_TYPE
#endif
#if !defined(RL_VECTOR3_TYPE)
typedef struct Vector3 { float x, y, z; } Vector3;
#define RL_VECTOR3_TYPE
#endif
#if !defined(RL_VECTOR4_TYPE)
typedef struct Vector4 { float x, y, z, w; } Vector4;
#define RL_VECTOR4_TYPE
#endif
#if !defined(RL_MATRIX_TYPE)
typedef struct Matrix {
    float m0, m4, m8, m12;
    float m1, m5, m9, m13;
    float m2, m6, m10, m14;
    float m3, m7, m11, m15;
} Matrix;
#define RL_MATRIX_TYPE
#endif

// Dynamic vertex buffers (position + texcoords + normals + colors + indices arrays)
typedef struct rlVertexBuffer {
    int elementCount;
    float *vertices;
    float *texcoords;
    float *normals;
    unsigned char *colors;
    unsigned int *indices;
    unsigned int vaoId;
    unsigned int vboId[5];
} rlVertexBuffer;

typedef struct rlDrawCall {
    int mode;
    int vertexCount;
    int vertexAlignment;
    unsigned int textureId;
} rlDrawCall;

typedef struct rlRenderBatch {
    int bufferCount;
    int currentBuffer;
    rlVertexBuffer *vertexBuffer;
    rlDrawCall *draws;
    int drawCounter;
    float currentDepth;
} rlRenderBatch;

//----------------------------------------------------------------------------------
// rlgl public function prototypes (the surface rcore.c / rmodels.c / ... call)
//----------------------------------------------------------------------------------
#if defined(__cplusplus)
extern "C" {
#endif

// Matrix
void rlMatrixMode(int mode);
void rlPushMatrix(void);
void rlPopMatrix(void);
void rlLoadIdentity(void);
void rlTranslatef(float x, float y, float z);
void rlRotatef(float angle, float x, float y, float z);
void rlScalef(float x, float y, float z);
void rlMultMatrixf(const float *matf);
void rlFrustum(double left, double right, double bottom, double top, double znear, double zfar);
void rlOrtho(double left, double right, double bottom, double top, double znear, double zfar);
void rlViewport(int x, int y, int width, int height);
void rlSetClipPlanes(double nearPlane, double farPlane);
double rlGetCullDistanceNear(void);
double rlGetCullDistanceFar(void);

// Vertex-level
void rlBegin(int mode);
void rlEnd(void);
void rlVertex2i(int x, int y);
void rlVertex2f(float x, float y);
void rlVertex3f(float x, float y, float z);
void rlTexCoord2f(float x, float y);
void rlNormal3f(float x, float y, float z);
void rlColor4ub(unsigned char r, unsigned char g, unsigned char b, unsigned char a);
void rlColor3f(float x, float y, float z);
void rlColor4f(float x, float y, float z, float w);

// Vertex buffers state
bool rlEnableVertexArray(unsigned int vaoId);
void rlDisableVertexArray(void);
void rlEnableVertexBuffer(unsigned int id);
void rlDisableVertexBuffer(void);
void rlEnableVertexBufferElement(unsigned int id);
void rlDisableVertexBufferElement(void);
void rlEnableVertexAttribute(unsigned int index);
void rlDisableVertexAttribute(unsigned int index);

// Textures state
void rlActiveTextureSlot(int slot);
void rlEnableTexture(unsigned int id);
void rlDisableTexture(void);
void rlEnableTextureCubemap(unsigned int id);
void rlDisableTextureCubemap(void);
void rlTextureParameters(unsigned int id, int param, int value);
void rlCubemapParameters(unsigned int id, int param, int value);

// Shader state
void rlEnableShader(unsigned int id);
void rlDisableShader(void);

// Framebuffer state
void rlEnableFramebuffer(unsigned int id);
void rlDisableFramebuffer(void);
unsigned int rlGetActiveFramebuffer(void);
void rlActiveDrawBuffers(int count);
void rlBlitFramebuffer(int srcX, int srcY, int srcWidth, int srcHeight, int dstX, int dstY, int dstWidth, int dstHeight, int bufferMask);
void rlBindFramebuffer(unsigned int target, unsigned int framebuffer);

// General render state
void rlEnableColorBlend(void);
void rlDisableColorBlend(void);
void rlEnableDepthTest(void);
void rlDisableDepthTest(void);
void rlEnableDepthMask(void);
void rlDisableDepthMask(void);
void rlEnableBackfaceCulling(void);
void rlDisableBackfaceCulling(void);
void rlColorMask(bool r, bool g, bool b, bool a);
void rlSetCullFace(int mode);
void rlEnableScissorTest(void);
void rlDisableScissorTest(void);
void rlScissor(int x, int y, int width, int height);
void rlEnableWireMode(void);
void rlEnablePointMode(void);
void rlDisableWireMode(void);
void rlSetLineWidth(float width);
float rlGetLineWidth(void);
void rlEnableSmoothLines(void);
void rlDisableSmoothLines(void);
void rlEnableStereoRender(void);
void rlDisableStereoRender(void);
bool rlIsStereoRenderEnabled(void);

void rlClearColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a);
void rlClearScreenBuffers(void);
void rlCheckErrors(void);
void rlSetBlendMode(int mode);
void rlSetBlendFactors(int glSrcFactor, int glDstFactor, int glEquation);
void rlSetBlendFactorsSeparate(int glSrcRGB, int glDstRGB, int glSrcAlpha, int glDstAlpha, int glEqRGB, int glEqAlpha);

// Stencil
void rlEnableStencilTest(void);
void rlDisableStencilTest(void);
void rlStencilFunc(int func, int ref, int mask);
void rlStencilOp(int fail, int zfail, int zpass);
void rlStencilMask(int mask);
void rlClearStencilBuffer(unsigned int value);

// rlgl init/de-init + render batch
void rlglInit(int width, int height);
void rlglClose(void);
void rlLoadExtensions(void *loader);
int rlGetVersion(void);
void rlSetFramebufferWidth(int width);
int rlGetFramebufferWidth(void);
void rlSetFramebufferHeight(int height);
int rlGetFramebufferHeight(void);
unsigned int rlGetTextureIdDefault(void);
unsigned int rlGetShaderIdDefault(void);
int *rlGetShaderLocsDefault(void);

rlRenderBatch rlLoadRenderBatch(int numBuffers, int bufferElements);
void rlUnloadRenderBatch(rlRenderBatch batch);
void rlDrawRenderBatch(rlRenderBatch *batch);
void rlSetRenderBatchActive(rlRenderBatch *batch);
void rlDrawRenderBatchActive(void);
bool rlCheckRenderBatchLimit(int vCount);
void rlSetTexture(unsigned int id);

// Vertex buffers management
unsigned int rlLoadVertexArray(void);
unsigned int rlLoadVertexBuffer(const void *buffer, int size, bool dynamic);
unsigned int rlLoadVertexBufferElement(const void *buffer, int size, bool dynamic);
void rlUpdateVertexBuffer(unsigned int bufferId, const void *data, int dataSize, int offset);
void rlUpdateVertexBufferElements(unsigned int id, const void *data, int dataSize, int offset);
void rlUnloadVertexArray(unsigned int vaoId);
void rlUnloadVertexBuffer(unsigned int vboId);
void rlSetVertexAttribute(unsigned int index, int compSize, int type, bool normalized, int stride, const void *pointer);
void rlSetVertexAttributeDivisor(unsigned int index, int divisor);
void rlSetVertexAttributeDefault(int locIndex, const void *value, int attribType, int count);
void rlDrawVertexArray(int offset, int count);
void rlDrawVertexArrayElements(int offset, int count, const void *buffer);
void rlDrawVertexArrayInstanced(int offset, int count, int instances);
void rlDrawVertexArrayElementsInstanced(int offset, int count, const void *buffer, int instances);

// Textures management
unsigned int rlLoadTexture(const void *data, int width, int height, int format, int mipmapCount);
unsigned int rlLoadTextureDepth(int width, int height, bool useRenderBuffer);
unsigned int rlLoadTextureCubemap(const void *data, int size, int format, int mipmapCount);
void rlUpdateTexture(unsigned int id, int offsetX, int offsetY, int width, int height, int format, const void *data);
void rlGetGlTextureFormats(int format, unsigned int *glInternalFormat, unsigned int *glFormat, unsigned int *glType);
const char *rlGetPixelFormatName(unsigned int format);
void rlUnloadTexture(unsigned int id);
void rlGenTextureMipmaps(unsigned int id, int width, int height, int format, int *mipmaps);
void *rlReadTexturePixels(unsigned int id, int width, int height, int format);
unsigned char *rlReadScreenPixels(int width, int height);

// Framebuffer management
unsigned int rlLoadFramebuffer(void);
void rlFramebufferAttach(unsigned int fboId, unsigned int texId, int attachType, int texType, int mipLevel);
bool rlFramebufferComplete(unsigned int id);
void rlUnloadFramebuffer(unsigned int id);

// Shaders management
unsigned int rlLoadShader(const char *code, int type);
unsigned int rlLoadShaderProgram(const char *vsCode, const char *fsCode);
unsigned int rlLoadShaderProgramEx(unsigned int vsId, unsigned int fsId);
unsigned int rlLoadShaderProgramCompute(unsigned int csId);
void rlUnloadShaderProgram(unsigned int id);
int rlGetLocationUniform(unsigned int shaderId, const char *uniformName);
int rlGetLocationAttrib(unsigned int shaderId, const char *attribName);
void rlSetUniform(int locIndex, const void *value, int uniformType, int count);
void rlSetUniformMatrix(int locIndex, Matrix mat);
void rlSetUniformMatrices(int locIndex, const Matrix *mat, int count);
void rlSetUniformSampler(int locIndex, unsigned int textureId);
void rlSetShader(unsigned int id, int *locs);

// Compute / SSBO (WebGPU supports compute; minimal pass-through here)
unsigned int rlLoadComputeShaderProgram(unsigned int shaderId);
void rlComputeShaderDispatch(unsigned int groupX, unsigned int groupY, unsigned int groupZ);
unsigned int rlLoadShaderBuffer(unsigned int size, const void *data, int usageHint);
void rlUnloadShaderBuffer(unsigned int ssboId);
void rlUpdateShaderBuffer(unsigned int id, const void *data, unsigned int dataSize, unsigned int offset);
void rlBindShaderBuffer(unsigned int id, unsigned int index);
void rlReadShaderBuffer(unsigned int id, void *dest, unsigned int count, unsigned int offset);
void rlCopyShaderBuffer(unsigned int destId, unsigned int srcId, unsigned int destOffset, unsigned int srcOffset, unsigned int count);
unsigned int rlGetShaderBufferSize(unsigned int id);
void rlBindImageTexture(unsigned int id, unsigned int index, int format, bool readonly);

// Matrix state
Matrix rlGetMatrixModelview(void);
Matrix rlGetMatrixProjection(void);
Matrix rlGetMatrixTransform(void);
Matrix rlGetMatrixProjectionStereo(int eye);
Matrix rlGetMatrixViewOffsetStereo(int eye);
void rlSetMatrixProjection(Matrix proj);
void rlSetMatrixModelview(Matrix view);
void rlSetMatrixProjectionStereo(Matrix right, Matrix left);
void rlSetMatrixViewOffsetStereo(Matrix right, Matrix left);

// Quick drawing
void rlLoadDrawCube(void);
void rlLoadDrawQuad(void);

// Frame cycle (backend-injected by the rcore overlay)
void rlBeginFrame(void);
void rlEndFrame(void);

// rlwg platform hooks (called by the web platform layer)
void rlwgSetSurface(WGPUSurface surface);
void rlwgSetDevice(WGPUDevice device, WGPUQueue queue);
void rlwgResize(int width, int height);

#if defined(__EMSCRIPTEN__)
// Asynchronous device acquisition (browser). The standalone example uses the
// blocking rlwgAcquireDevice() path (requires -sASYNCIFY). An engine host that
// must NOT pull in ASYNCIFY (e.g. to allow -sUSE_PTHREADS) instead calls this:
// it creates the instance/surface, requests adapter then device via spontaneous
// callbacks, sets RLWG.device/queue/surface, and finally invokes `cb(user)` once
// the device is ready. The host should boot the engine / start its rAF loop from
// that callback. No emscripten_sleep, no ASYNCIFY.
typedef void (*rlwgDeviceReadyCb)(void *user);
void rlwgAcquireDeviceAsync(const char *canvasSelector, rlwgDeviceReadyCb cb, void *user);
#endif

// True once a WGPU device is available (set by either acquire path or rlwgSetDevice).
// The web platform layer uses this to skip its own acquisition when an engine host
// has already provided the device asynchronously.
bool rlwgHasDevice(void);

#if defined(__cplusplus)
}
#endif

//==================================================================================
//                                  IMPLEMENTATION
//==================================================================================

#define RLWG_MAX_TEXTURE_SLOTS  RL_DEFAULT_BATCH_MAX_TEXTURE_UNITS

// Interleaved batch vertex (the default-shader vertex layout, stride 36 bytes)
typedef struct RLWGBatchVertex {
    float position[3];   // loc 0
    float texcoord[2];   // loc 1
    float normal[3];     // loc 2
    unsigned char color[4]; // loc 3 (unorm8x4)
} RLWGBatchVertex;

// CPU mirror of the default-shader uniform blocks (see rlwg_default_wgsl.h).
// rlSetUniform()/rlSetUniformMatrix() write into this by byte offset.
typedef struct RLWGVertexUBO {     // 320 bytes
    Matrix mvp, view, projection, model, normalMat;
} RLWGVertexUBO;
typedef struct RLWGFragUBO {       // 64 bytes
    Vector4 colDiffuse;
    Vector4 colorSpecular;
    Vector4 vectorView;            // xyz used
    Vector2 resolution;
    float radius;
    float _pad;
} RLWGFragUBO;

typedef struct RLWGShaderUniformBlock {
    RLWGVertexUBO v;
    RLWGFragUBO f;
} RLWGShaderUniformBlock;

typedef struct RLWGTextureRecord {
    bool used;
    unsigned int id;
    int width, height, format, mipmaps;
    bool isCubemap;
    WGPUTexture texture;
    WGPUTextureView view;
    WGPUSampler sampler;
} RLWGTextureRecord;

typedef struct RLWGShaderRecord {
    bool used;
    unsigned int id;
    bool isDefault;
    WGPUShaderModule module;          // single module holding vs_main + fs_main
    WGPUBindGroupLayout bindGroupLayout;
    WGPUPipelineLayout pipelineLayout;
    // Uniform buffers (@binding 0/1) are allocated per-flush, not per-shader.
    char vsEntry[16], fsEntry[16];
    int attribLocs[RL_MAX_SHADER_LOCATIONS];
} RLWGShaderRecord;

typedef struct RLWGFramebufferRecord {
    bool used;
    unsigned int id;
    int width, height;
    unsigned int colorTex;
    unsigned int depthTex;
} RLWGFramebufferRecord;

typedef struct RLWGVertexBufferRecord {
    bool used;
    unsigned int id;
    WGPUBuffer buffer;
    size_t size;
    bool element;
} RLWGVertexBufferRecord;

typedef struct rlwgData {
    bool inited;
    bool vSync;

    // WebGPU core
    WGPUInstance instance;
    WGPUAdapter adapter;
    WGPUDevice device;
    WGPUQueue queue;
    WGPUSurface surface;
    WGPUTextureFormat surfaceFormat;
    WGPUTextureFormat depthFormat;

    // Per-frame
    WGPUTextureView currentColorView;   // acquired swapchain view (or active FBO color view)
    WGPUTextureView currentDepthView;
    WGPUCommandEncoder encoder;
    WGPURenderPassEncoder pass;
    bool passOpen;
    bool mainPassCleared;               // has the backbuffer been cleared this frame?

    // Depth buffer for the backbuffer (multisampled — see RLWG_SAMPLE_COUNT)
    WGPUTexture depthTexture;
    WGPUTextureView depthTextureView;
    int depthW, depthH;

    // Multisample (MSAA) color target for the backbuffer; resolved into the swapchain
    // texture at pass end so shape/icon edges are antialiased (parity with rlvk/rlmt).
    WGPUTexture msaaColorTexture;
    WGPUTextureView msaaColorView;
    int msaaW, msaaH;

    WGPUColor clearColor;

    // CPU batch accumulation (GPU vertex buffers are allocated per-flush, see rlwgFlush)
    RLWGBatchVertex *frameVertices;
    int frameVertexCount;
    int frameVertexCapacity;

    struct {
        int mode;
        int startVertex;
        int vertexCount;
        unsigned int textureId;
        unsigned int shaderId;
    } *framePrimitives;
    int framePrimitiveCount;
    int framePrimitiveCapacity;

    // Object tables
    RLWGTextureRecord *textures;     int textureCapacity;     unsigned int nextTextureId;
    RLWGShaderRecord *shaders;       int shaderCapacity;      unsigned int nextShaderId;
    RLWGFramebufferRecord *framebuffers; int framebufferCapacity; unsigned int nextFramebufferId;
    RLWGVertexBufferRecord *vertexBuffers; int vertexBufferCapacity; unsigned int nextVertexBufferId;

    // Pipeline cache (key derived from shader/blend/depth/cull/topology/format)
    struct RLWGPipelineEntry { uint64_t key; WGPURenderPipeline pipeline; } *pipelineCache;
    int pipelineCacheCount, pipelineCacheCapacity;

    // Transient per-flush uniform buffers. A single shared UBO cannot be reused across
    // flushes in one frame: wgpuQueueWriteBuffer writes all land before the command
    // buffer executes, so every draw would read the LAST matrices written. Each flush
    // therefore gets its own UBO pair, all released after submit in rlEndFrame.
    WGPUBuffer *frameUBOs; int frameUBOCount, frameUBOCapacity;

    rlRenderBatch *currentBatch;

    struct {
        int vertexCounter;
        float texcoordx, texcoordy;
        float normalx, normaly, normalz;
        unsigned char colorr, colorg, colorb, colora;

        int currentMatrixMode;
        Matrix *currentMatrix;
        Matrix modelview;
        Matrix projection;
        Matrix transform;
        bool transformRequired;
        Matrix stack[RL_MAX_MATRIX_STACK_SIZE];
        int stackCounter;

        unsigned int defaultTextureId;
        unsigned int activeTextureId[RLWG_MAX_TEXTURE_SLOTS];
        unsigned int defaultShaderId;
        int *defaultShaderLocs;
        unsigned int currentShaderId;
        int *currentShaderLocs;

        bool stereoRender;
        Matrix projectionStereo[2];
        Matrix viewOffsetStereo[2];

        int currentBlendMode;
        int glBlendSrcFactor, glBlendDstFactor, glBlendEquation;
        int glBlendSrcFactorRGB, glBlendDestFactorRGB, glBlendSrcFactorAlpha, glBlendDestFactorAlpha;
        int glBlendEquationRGB, glBlendEquationAlpha;
        bool glCustomBlendModeModified;

        int framebufferWidth, framebufferHeight;
        int currentDrawMode;
        bool depthTestEnabled;
        bool depthWriteEnabled;
        bool cullEnabled;
        bool colorMaskR, colorMaskG, colorMaskB, colorMaskA;
        unsigned int currentTextureId;
        unsigned int currentShaderActive;
    } State;

    struct {
        bool vao, instancing, texNPOT, texDepth, texFloat32, texFloat16;
        bool texCompDXT, texCompETC1, texCompETC2, texCompPVRT, texCompASTC;
        bool texMirrorClamp, texAnisoFilter, computeShader, ssbo;
        float maxAnisotropyLevel;
        int maxDepthBits;
    } ExtSupported;

    unsigned int activeFramebufferId;
} rlwgData;

#if defined(__cplusplus)
    #define RLWG_THREAD_LOCAL thread_local
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
    #define RLWG_THREAD_LOCAL _Thread_local
#else
    #define RLWG_THREAD_LOCAL
#endif

static rlwgData RLWG_DefaultContext = { 0 };
static RLWG_THREAD_LOCAL rlwgData *RLWG_CurrentContext = &RLWG_DefaultContext;
#define RLWG (*RLWG_CurrentContext)

static inline rlwgData *rlwgGetContext(void) { return RLWG_CurrentContext; }
static inline void rlwgSetContext(rlwgData *ctx) { RLWG_CurrentContext = ctx ? ctx : &RLWG_DefaultContext; }

static double RLWG_CullDistanceNear = RL_CULL_DISTANCE_NEAR;
static double RLWG_CullDistanceFar = RL_CULL_DISTANCE_FAR;
static float RLWG_LineWidth = 1.0f;

//----------------------------------------------------------------------------------
// Matrix math (local copies - pure CPU, no GPU dependency)
//----------------------------------------------------------------------------------
static Matrix rlwgMatrixIdentity(void)
{
    Matrix r = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
    return r;
}

static Matrix rlwgMatrixMultiply(Matrix left, Matrix right)
{
    Matrix result;
    result.m0  = left.m0*right.m0  + left.m1*right.m4  + left.m2*right.m8   + left.m3*right.m12;
    result.m1  = left.m0*right.m1  + left.m1*right.m5  + left.m2*right.m9   + left.m3*right.m13;
    result.m2  = left.m0*right.m2  + left.m1*right.m6  + left.m2*right.m10  + left.m3*right.m14;
    result.m3  = left.m0*right.m3  + left.m1*right.m7  + left.m2*right.m11  + left.m3*right.m15;
    result.m4  = left.m4*right.m0  + left.m5*right.m4  + left.m6*right.m8   + left.m7*right.m12;
    result.m5  = left.m4*right.m1  + left.m5*right.m5  + left.m6*right.m9   + left.m7*right.m13;
    result.m6  = left.m4*right.m2  + left.m5*right.m6  + left.m6*right.m10  + left.m7*right.m14;
    result.m7  = left.m4*right.m3  + left.m5*right.m7  + left.m6*right.m11  + left.m7*right.m15;
    result.m8  = left.m8*right.m0  + left.m9*right.m4  + left.m10*right.m8  + left.m11*right.m12;
    result.m9  = left.m8*right.m1  + left.m9*right.m5  + left.m10*right.m9  + left.m11*right.m13;
    result.m10 = left.m8*right.m2  + left.m9*right.m6  + left.m10*right.m10 + left.m11*right.m14;
    result.m11 = left.m8*right.m3  + left.m9*right.m7  + left.m10*right.m11 + left.m11*right.m15;
    result.m12 = left.m12*right.m0 + left.m13*right.m4 + left.m14*right.m8  + left.m15*right.m12;
    result.m13 = left.m12*right.m1 + left.m13*right.m5 + left.m14*right.m9  + left.m15*right.m13;
    result.m14 = left.m12*right.m2 + left.m13*right.m6 + left.m14*right.m10 + left.m15*right.m14;
    result.m15 = left.m12*right.m3 + left.m13*right.m7 + left.m14*right.m11 + left.m15*right.m15;
    return result;
}

// raylib stores Matrix column-major in memory layout (m0,m1,m2,m3 = first column).
// WGSL mat4x4<f32> is column-major too, so a raw memcpy of the 16 floats is correct.
static void rlwgMatrixToFloats(Matrix m, float *out16)
{
    out16[0]=m.m0;  out16[1]=m.m1;  out16[2]=m.m2;  out16[3]=m.m3;
    out16[4]=m.m4;  out16[5]=m.m5;  out16[6]=m.m6;  out16[7]=m.m7;
    out16[8]=m.m8;  out16[9]=m.m9;  out16[10]=m.m10; out16[11]=m.m11;
    out16[12]=m.m12; out16[13]=m.m13; out16[14]=m.m14; out16[15]=m.m15;
}

//----------------------------------------------------------------------------------
// Capacity helpers
//----------------------------------------------------------------------------------
static bool rlwgEnsureFrameVertexCapacity(int needed)
{
    if (needed <= RLWG.frameVertexCapacity) return true;
    int newCap = RLWG.frameVertexCapacity > 0 ? RLWG.frameVertexCapacity : (RL_DEFAULT_BATCH_BUFFER_ELEMENTS*4);
    while (newCap < needed) newCap *= 2;
    RLWGBatchVertex *grown = (RLWGBatchVertex *)RL_REALLOC(RLWG.frameVertices, (size_t)newCap*sizeof(RLWGBatchVertex));
    if (!grown) { TRACELOG(RL_LOG_ERROR, "RLWG: grow vertex buffer failed (%d)", newCap); return false; }
    RLWG.frameVertices = grown;
    RLWG.frameVertexCapacity = newCap;
    return true;
}

static bool rlwgEnsureFramePrimitiveCapacity(int needed)
{
    if (needed <= RLWG.framePrimitiveCapacity) return true;
    int newCap = RLWG.framePrimitiveCapacity > 0 ? RLWG.framePrimitiveCapacity : RL_DEFAULT_BATCH_DRAWCALLS;
    while (newCap < needed) newCap *= 2;
    void *grown = RL_REALLOC(RLWG.framePrimitives, (size_t)newCap*sizeof(*RLWG.framePrimitives));
    if (!grown) return false;
    RLWG.framePrimitives = grown;
    RLWG.framePrimitiveCapacity = newCap;
    return true;
}

//----------------------------------------------------------------------------------
// Record table helpers
//----------------------------------------------------------------------------------
static RLWGTextureRecord *rlwgGetTexture(unsigned int id)
{
    for (int i = 0; i < RLWG.textureCapacity; i++)
        if (RLWG.textures[i].used && RLWG.textures[i].id == id) return &RLWG.textures[i];
    return NULL;
}
static RLWGTextureRecord *rlwgAllocTexture(void)
{
    for (int i = 0; i < RLWG.textureCapacity; i++)
        if (!RLWG.textures[i].used) { memset(&RLWG.textures[i], 0, sizeof(RLWGTextureRecord)); RLWG.textures[i].used = true; return &RLWG.textures[i]; }
    int old = RLWG.textureCapacity;
    int nc = old > 0 ? old*2 : 64;
    RLWG.textures = (RLWGTextureRecord *)RL_REALLOC(RLWG.textures, (size_t)nc*sizeof(RLWGTextureRecord));
    memset(&RLWG.textures[old], 0, (size_t)(nc-old)*sizeof(RLWGTextureRecord));
    RLWG.textureCapacity = nc;
    RLWG.textures[old].used = true;
    return &RLWG.textures[old];
}
static RLWGShaderRecord *rlwgGetShader(unsigned int id)
{
    for (int i = 0; i < RLWG.shaderCapacity; i++)
        if (RLWG.shaders[i].used && RLWG.shaders[i].id == id) return &RLWG.shaders[i];
    return NULL;
}
static RLWGShaderRecord *rlwgAllocShader(void)
{
    for (int i = 0; i < RLWG.shaderCapacity; i++)
        if (!RLWG.shaders[i].used) { memset(&RLWG.shaders[i], 0, sizeof(RLWGShaderRecord)); RLWG.shaders[i].used = true; return &RLWG.shaders[i]; }
    int old = RLWG.shaderCapacity;
    int nc = old > 0 ? old*2 : 16;
    RLWG.shaders = (RLWGShaderRecord *)RL_REALLOC(RLWG.shaders, (size_t)nc*sizeof(RLWGShaderRecord));
    memset(&RLWG.shaders[old], 0, (size_t)(nc-old)*sizeof(RLWGShaderRecord));
    RLWG.shaderCapacity = nc;
    RLWG.shaders[old].used = true;
    return &RLWG.shaders[old];
}
static RLWGVertexBufferRecord *rlwgGetVB(unsigned int id)
{
    for (int i = 0; i < RLWG.vertexBufferCapacity; i++)
        if (RLWG.vertexBuffers[i].used && RLWG.vertexBuffers[i].id == id) return &RLWG.vertexBuffers[i];
    return NULL;
}
static RLWGVertexBufferRecord *rlwgAllocVB(void)
{
    for (int i = 0; i < RLWG.vertexBufferCapacity; i++)
        if (!RLWG.vertexBuffers[i].used) { memset(&RLWG.vertexBuffers[i], 0, sizeof(RLWGVertexBufferRecord)); RLWG.vertexBuffers[i].used = true; return &RLWG.vertexBuffers[i]; }
    int old = RLWG.vertexBufferCapacity;
    int nc = old > 0 ? old*2 : 64;
    RLWG.vertexBuffers = (RLWGVertexBufferRecord *)RL_REALLOC(RLWG.vertexBuffers, (size_t)nc*sizeof(RLWGVertexBufferRecord));
    memset(&RLWG.vertexBuffers[old], 0, (size_t)(nc-old)*sizeof(RLWGVertexBufferRecord));
    RLWG.vertexBufferCapacity = nc;
    RLWG.vertexBuffers[old].used = true;
    return &RLWG.vertexBuffers[old];
}

//----------------------------------------------------------------------------------
// Format mapping (rlPixelFormat -> WGPUTextureFormat) - GL 3.3 compatible subset
//----------------------------------------------------------------------------------
static WGPUTextureFormat rlwgToWGPUFormat(int rlFormat)
{
    switch (rlFormat)
    {
        case RL_PIXELFORMAT_UNCOMPRESSED_GRAYSCALE:   return WGPUTextureFormat_R8Unorm;
        case RL_PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA:  return WGPUTextureFormat_RG8Unorm;
        case RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8:    return WGPUTextureFormat_RGBA8Unorm;
        case RL_PIXELFORMAT_UNCOMPRESSED_R32:         return WGPUTextureFormat_R32Float;
        case RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32A32:return WGPUTextureFormat_RGBA32Float;
        case RL_PIXELFORMAT_UNCOMPRESSED_R16:         return WGPUTextureFormat_R16Float;
        case RL_PIXELFORMAT_UNCOMPRESSED_R16G16B16A16:return WGPUTextureFormat_RGBA16Float;
        // 24bpp RGB and packed 16bpp formats have no direct WebGPU equivalent;
        // raylib's rtextures.c expands RGB->RGBA before upload in most paths.
        case RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8:      return WGPUTextureFormat_RGBA8Unorm;
        default:                                      return WGPUTextureFormat_RGBA8Unorm;
    }
}

static int rlwgPixelByteSize(int rlFormat)
{
    switch (rlFormat)
    {
        case RL_PIXELFORMAT_UNCOMPRESSED_GRAYSCALE:   return 1;
        case RL_PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA:  return 2;
        case RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8:    return 4;
        case RL_PIXELFORMAT_UNCOMPRESSED_R32:         return 4;
        case RL_PIXELFORMAT_UNCOMPRESSED_R32G32B32A32:return 16;
        case RL_PIXELFORMAT_UNCOMPRESSED_R16:         return 2;
        case RL_PIXELFORMAT_UNCOMPRESSED_R16G16B16A16:return 8;
        default:                                      return 4;
    }
}

//----------------------------------------------------------------------------------
// Blend-mode -> WGPUBlendState (GL 3.3 raylib blend semantics)
//----------------------------------------------------------------------------------
static WGPUBlendState rlwgBlendStateForMode(int mode)
{
    WGPUBlendState bs = {0};
    switch (mode)
    {
        case RL_BLEND_ADDITIVE:
            bs.color.srcFactor = WGPUBlendFactor_SrcAlpha; bs.color.dstFactor = WGPUBlendFactor_One; bs.color.operation = WGPUBlendOperation_Add;
            bs.alpha = bs.color; break;
        case RL_BLEND_MULTIPLIED:
            bs.color.srcFactor = WGPUBlendFactor_Dst; bs.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha; bs.color.operation = WGPUBlendOperation_Add;
            bs.alpha = bs.color; break;
        case RL_BLEND_ADD_COLORS:
            bs.color.srcFactor = WGPUBlendFactor_One; bs.color.dstFactor = WGPUBlendFactor_One; bs.color.operation = WGPUBlendOperation_Add;
            bs.alpha = bs.color; break;
        case RL_BLEND_SUBTRACT_COLORS:
            bs.color.srcFactor = WGPUBlendFactor_One; bs.color.dstFactor = WGPUBlendFactor_One; bs.color.operation = WGPUBlendOperation_ReverseSubtract;
            bs.alpha = bs.color; break;
        case RL_BLEND_ALPHA_PREMULTIPLY:
            bs.color.srcFactor = WGPUBlendFactor_One; bs.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha; bs.color.operation = WGPUBlendOperation_Add;
            bs.alpha = bs.color; break;
        case RL_BLEND_ALPHA:
        default:
            bs.color.srcFactor = WGPUBlendFactor_SrcAlpha; bs.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha; bs.color.operation = WGPUBlendOperation_Add;
            bs.alpha.srcFactor = WGPUBlendFactor_One; bs.alpha.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha; bs.alpha.operation = WGPUBlendOperation_Add;
            break;
    }
    return bs;
}

static WGPUStringView rlwgStr(const char *s)
{
    WGPUStringView v; v.data = s; v.length = s ? strlen(s) : 0; return v;
}

#include "rlwg_impl.h"

#endif // RLGL_H
