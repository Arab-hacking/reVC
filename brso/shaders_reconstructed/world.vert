// ============================================================
// RECONSTRUCTION of the world VERTEX shader (libblackrussia-client.so)
// Assembled from the composer at 0x113b3dc (strcat chain over .rodata pieces).
// Conditional pieces (permutation) are wrapped in // --- IF ... --- comments.
// ============================================================
#version 300 es
#define VSIN(index) layout(location = index) in
#define VSOUT out
#define FSIN in

precision highp float;
precision highp int;

#define ATTRIB_POS         0
#define ATTRIB_NORMAL      1
#define ATTRIB_COLOR       2
#define ATTRIB_WEIGHTS     3
#define ATTRIB_INDICES     4
#define ATTRIB_TEXCOORDS0  5
#define ATTRIB_TEXCOORDS1  6

VSIN(ATTRIB_POS)    vec4 in_pos;      // or vec3
VSIN(ATTRIB_NORMAL)       vec3 in_normal;
VSIN(ATTRIB_COLOR)         vec4 in_color;
VSIN(ATTRIB_WEIGHTS)       vec4 in_weights;
VSIN(ATTRIB_INDICES)       vec4 in_indices;
VSIN(ATTRIB_TEXCOORDS0)    vec2 in_tex0;
VSIN(ATTRIB_TEXCOORDS1)    vec2 in_tex1;

uniform highp mat4 u_proj;
uniform highp mat4 u_view;
uniform highp mat4 u_world;
uniform highp mat4 u_uvMatrix;
uniform mediump vec4 u_materialAmbient;
uniform mediump vec2 u_fogData;          // fogStart, fogScale
uniform mediump vec4 u_surfProps;        // amb, spec, diff, extra
#define surfAmbient (u_surfProps.x)
#define surfSpecular (u_surfProps.y)
#define surfDiffuse (u_surfProps.z)

uniform mediump vec4 u_clipPlane;
uniform highp vec3 u_eye;
uniform highp vec3 u_sunDir;
uniform float u_outlineThickness;

VSOUT mediump vec4 v_color;
VSOUT mediump float v_fog;
VSOUT highp vec4 v_ndc;
VSOUT highp vec2 v_tex0;
VSOUT highp vec2 v_tex1;
VSOUT highp vec2 v_textureCoords;
const float tiling = 0.25;
VSOUT highp vec3 v_worldPosition;
VSOUT highp vec3 v_worldNormal;
VSOUT mediump float diffuse;
VSOUT highp vec3 v_eyePos;
VSOUT highp vec3 v_eyeNormal;
VSOUT highp vec2 v_tileUv;
VSOUT highp vec2 v_snowTileUv;
uniform highp float u_snowCoverTileMult;
// --- IF INSTANCING ---
uniform highp mat4 u_worldInstancing[40];
uniform mediump vec4 u_materialAmbientInstancing[40];
// --- IF SHADOWS ---
uniform highp mat4 u_projLight;
uniform highp mat4 u_viewLight;
// --- IF DEBUG_WEIGHTS ---
uniform mediump float u_colorInterp;
// --- IF SKINNED ---
uniform highp mat4 u_boneMatrices[32];
// --- IF 2D-PROJ (radar/im2d) ---
uniform highp vec4 u_xform;
// --- IF CLIP ---
VSOUT mediump float clipDistance;
// --- IF SHADOWS ---
VSOUT highp vec3 v_shadowProj;

void main(void) {
  highp vec4 Vertex = vec4(0.0, 0.0, 0.0, 0.0);
  highp vec3 Normal = vec3(0.0, 0.0, 0.0);

  Vertex = u_world * vec4(in_pos, 1.0);
  // --- IF INSTANCING --- Vertex = u_worldInstancing[gl_InstanceID] * vec4(in_pos, 1.0);
  Normal = mat3(u_world) * in_normal;
  vec4 ViewPos = u_view * Vertex;
  gl_Position = u_proj * ViewPos;

  // --- IF OUTLINE --- Vertex.xyz = Vertex.xyz + Normal * u_outlineThickness;

  // --- IF SKINNED ---
  highp vec3 SkinVertex = vec3(0.0, 0.0, 0.0);
  highp vec3 SkinNormal = vec3(0.0, 0.0, 0.0);
  for (int i = 0; i < 4; i++) {
      SkinVertex += (u_boneMatrices[int(in_indices[i])] * vec4(in_pos, 1.0)).xyz * in_weights[i];
      SkinNormal += (mat3(u_boneMatrices[int(in_indices[i])]) * in_normal) * in_weights[i];
  }
  Normal = mat3(u_world) * SkinNormal;
  Vertex = u_world * vec4(SkinVertex, 1.0);
  gl_Position = u_proj * u_view * Vertex;
  // --- END ---

  // --- IF REFLECTION-PROBE ---
  vec3 ReflVector = Vertex.xyz - u_eye.xyz;
  vec3 ReflPos = normalize(ReflVector);
  ReflPos.xy = normalize(ReflPos.xy) * (ReflPos.z * 0.5 + 0.5);
  gl_Position = vec4(ReflPos.xy, length(ReflVector) * 0.002, 1.0);

  v_worldPosition = Vertex.xyz;
  v_worldNormal = normalize(Normal);
  v_ndc = gl_Position;
  v_textureCoords = vec2(in_pos.x / 2.0 + 0.5, in_pos.y / 2.0 + 0.5) * tiling;
  diffuse = max(dot(normalize(Normal), normalize(-u_sunDir)), 0.0);
  v_tex0 = (u_uvMatrix * vec4(in_tex0, 0.0, 1.0)).xy;
  v_color = in_color;
  // --- IF DEBUG_WEIGHTS --- v_color = mix(in_weights, v_color, u_colorInterp);
  v_eyeNormal = mat3(u_view) * Normal;
  v_eyePos = (u_view * Vertex).xyz;
  // --- extras: --- v_color = vec4(normalize(Normal), 1.0);   (normal-debug permutation)
  v_tileUv = Vertex.xy / 5.0;
  v_snowTileUv = Vertex.xy * u_snowCoverTileMult;
  clipDistance  = dot(Vertex, u_clipPlane);
  vec4 shadowProj = u_projLight * u_viewLight * Vertex;
  mediump float bias = max(0.01 * pow(1.0 - dot(normalize(Normal), normalize(-u_sunDir)), 4.0), 0.0001);
  shadowProj.z -= bias;
  shadowProj /= shadowProj.w;
  v_shadowProj = shadowProj.xyz * 0.5 + 0.5;
  v_tex1 = in_tex1;
  v_fog = clamp((length(Vertex.xyz - u_eye.xyz) - u_fogData.x) * u_fogData.y, 0.0, 1.0);
  // ambient add (instancing variant uses per-instance)
  v_color.rgb += u_materialAmbient.rgb;
  v_color.a *= u_materialAmbient.a;
  v_color = clamp(v_color, 0.0, 1.0);
  // --- IF DIFFUSE-FEEDBACK (permutation) --- v_color.rgb += v_color.rgb * diffuse;
}
