/**********************************************************************************************
*
*   rlwg_default_wgsl.h - Default WGSL shaders for the rlwg WebGPU backend
*
*   These are the raylib OpenGL 3.3 default vertex/fragment shaders, hand-translated
*   to WGSL so that the *browser* WebGPU runtime (which accepts WGSL only - no SPIR-V)
*   can run them. The translation is 1:1 with the GL 3.3 defaults, with exactly two
*   intentional WebGPU deviations, both invisible to rlgl callers:
*
*     1. Loose GL uniforms (`uniform mat4 mvp;`, `uniform vec4 colDiffuse;`, ...) are
*        packed into uniform buffers, because WebGPU has no loose/global uniforms.
*        The C-side mirror struct RLWGShaderUniformBlock (in rlwg.h) matches these
*        layouts byte-for-byte so rlSetUniform()/rlSetUniformMatrix() write by offset.
*
*     2. WebGPU clip space uses z in [0, 1] while OpenGL uses z in [-1, 1]. The default
*        vertex shader remaps it with `pos.z = (pos.z + pos.w)*0.5;`. Custom WGSL
*        shaders ported from GL 3.3 must include the same line (see DOCS).
*
*   Binding model (group 0):
*     @binding(0)  var<uniform> vertexUBO   - mvp/view/projection/model/normal
*     @binding(1)  var<uniform> fragUBO     - colDiffuse/colorSpecular/vectorView/...
*     @binding(2)  texture_2d<f32> texture0 - SHADER_LOC_MAP_DIFFUSE
*     @binding(3)  sampler texture0Smp
*
**********************************************************************************************/

#ifndef RLWG_DEFAULT_WGSL_H
#define RLWG_DEFAULT_WGSL_H

/* Reference GL 3.3 default shaders this is translated from (rlgl / rlvk / rlmt):
 *   VS: in vec3 vertexPosition; in vec2 vertexTexCoord; in vec4 vertexColor;
 *       uniform mat4 mvp; out vec2 fragTexCoord; out vec4 fragColor;
 *       gl_Position = mvp*vec4(vertexPosition, 1.0);
 *   FS: uniform sampler2D texture0; uniform vec4 colDiffuse;
 *       finalColor = texture(texture0, fragTexCoord)*colDiffuse*fragColor;
 */
static const char *rlwgDefaultShaderWGSL =
"struct VertexUBO {\n"
"    mvp        : mat4x4<f32>,\n"   // offset   0  (SHADER_LOC_MATRIX_MVP)
"    view       : mat4x4<f32>,\n"   // offset  64  (SHADER_LOC_MATRIX_VIEW)
"    projection : mat4x4<f32>,\n"   // offset 128  (SHADER_LOC_MATRIX_PROJECTION)
"    model      : mat4x4<f32>,\n"   // offset 192  (SHADER_LOC_MATRIX_MODEL)
"    normalMat  : mat4x4<f32>,\n"   // offset 256  (SHADER_LOC_MATRIX_NORMAL)
"};\n"
"struct FragUBO {\n"
"    colDiffuse    : vec4<f32>,\n"  // offset   0  (SHADER_LOC_COLOR_DIFFUSE)
"    colorSpecular : vec4<f32>,\n"  // offset  16  (SHADER_LOC_COLOR_SPECULAR)
"    vectorView    : vec4<f32>,\n"  // offset  32  (SHADER_LOC_VECTOR_VIEW, xyz)
"    resolution    : vec2<f32>,\n"  // offset  48
"    radius        : f32,\n"        // offset  56
"    _pad          : f32,\n"        // offset  60  -> struct size 64
"};\n"
"@group(0) @binding(0) var<uniform> vu : VertexUBO;\n"
"@group(0) @binding(1) var<uniform> fu : FragUBO;\n"
"@group(0) @binding(2) var texture0 : texture_2d<f32>;\n"
"@group(0) @binding(3) var texture0Smp : sampler;\n"
"\n"
"struct VSIn {\n"
"    @location(0) position : vec3<f32>,\n"   // vertexPosition  (rlgl loc 0)
"    @location(1) texcoord : vec2<f32>,\n"   // vertexTexCoord  (rlgl loc 1)
"    @location(2) normal   : vec3<f32>,\n"   // vertexNormal    (rlgl loc 2)
"    @location(3) color    : vec4<f32>,\n"   // vertexColor     (rlgl loc 3, u8 norm)
"};\n"
"struct VSOut {\n"
"    @builtin(position) position : vec4<f32>,\n"
"    @location(0) fragTexCoord : vec2<f32>,\n"
"    @location(1) fragColor : vec4<f32>,\n"
"};\n"
"\n"
"@vertex\n"
"fn vs_main(in : VSIn) -> VSOut {\n"
"    var out : VSOut;\n"
"    var p : vec4<f32> = vu.mvp * vec4<f32>(in.position, 1.0);\n"
"    p.z = (p.z + p.w) * 0.5;\n"   // GL z[-1,1] -> WebGPU z[0,1]
"    out.position = p;\n"
"    out.fragTexCoord = in.texcoord;\n"
"    out.fragColor = in.color;\n"
"    return out;\n"
"}\n"
"\n"
"@fragment\n"
"fn fs_main(in : VSOut) -> @location(0) vec4<f32> {\n"
"    let texelColor = textureSample(texture0, texture0Smp, in.fragTexCoord);\n"
"    return texelColor * fu.colDiffuse * in.fragColor;\n"
"}\n";

#endif // RLWG_DEFAULT_WGSL_H
