// ============================================================
// RECONSTRUCTION of the world FRAGMENT shader (libblackrussia-client.so)
// Assembled from the composer at 0x113c3b0 (strcat chain over .rodata pieces).
// Conditional pieces (permutation) are wrapped in // --- IF ... --- comments.
// ============================================================
#version 300 es
#define VSOUT out
#define FSIN in
#define FRAGCOLOR(x) fragColor = x

precision mediump float;
precision mediump int;

uniform sampler2D tex0;
uniform sampler2D tex1;          // emissive / lightmap (per permutation)
uniform mediump float u_emissiveSaturation;
uniform mediump vec4 u_pbrModelEmissionColorAlpha;
uniform mediump float u_pbrModelEmissionStrength;
uniform mediump float u_lightmapMulti;
// --- IF TERRAIN ---
uniform sampler2D tex2;
uniform sampler2D tex4;
uniform sampler2D tex6;
uniform sampler2D tex7;
uniform highp vec4 u_terrainTileMult;
// --- IF SHADOWS ---
uniform highp sampler2DShadow tex5;
// --- IF WATER ---
uniform sampler2D tex8;
uniform float u_moveFactor;
const float waveStrength = 0.015;

uniform mediump vec4 u_fogColor;
out vec4 fragColor;
uniform mediump vec4 u_debugColor;
uniform mediump float u_smoothAlphaThreshold;
uniform highp vec3 u_eye;
uniform highp vec3 u_sunPos;
uniform highp vec3 u_sunDir;
uniform mediump float u_localInvGamma;
uniform vec4 u_outlineColor;
// --- IF SNOW ---
uniform sampler2D tex3;
uniform mediump float u_snowEnable;
uniform mediump float u_snowCoverAmount;
uniform mediump vec4 u_snowCoverColor;
// --- IF CLIP-RECT (radar/UI) ---
uniform mediump vec4 u_clipRectangleInner;
uniform mediump vec4 u_clipRectangleOuter;
// --- IF ENVMAP/MATFX ---
uniform sampler2D tex2;
uniform float u_envMapCoef;

FSIN highp vec4 v_ndc;
FSIN highp vec2 v_textureCoords;
FSIN highp vec3 v_worldPosition;
FSIN highp vec3 v_worldNormal;
FSIN mediump float clipDistance;
FSIN mediump float diffuse;
FSIN highp vec3 v_shadowProj;
FSIN float v_fog;
FSIN vec4 v_color;
FSIN vec2 v_tex0;
FSIN vec2 v_tex1;
FSIN vec3 v_eyePos;
FSIN vec3 v_eyeNormal;
FSIN highp vec2 v_tileUv;
FSIN highp vec2 v_snowTileUv;

void DoThresholdAlphaTest(float a, float threshold) {
    if (a < threshold) discard;
}
void DoAlphaTest(float a) {
    DoThresholdAlphaTest(a, 0.5);
}

void main(void) {
  if (clipDistance < 0.0) discard;

  // ---- color source (permutation) ----
  mediump vec4 color = texture(tex0, v_tex0);
  // --- IF TERRAIN ---
  vec4 terrainMask = texture(tex7, vec2(v_tex0.x, 1.0 + v_tex0.y));
  vec4 layer0 = texture(tex1, v_tileUv * u_terrainTileMult.x);
  vec4 layer1 = texture(tex2, v_tileUv * u_terrainTileMult.y);
  vec4 layer2 = texture(tex4, v_tileUv * u_terrainTileMult.z);
  vec4 layer3 = texture(tex6, v_tileUv * u_terrainTileMult.w);
  color = vec4(layer0.rgb * terrainMask.r, 1.0);
  color.rgb += layer1.rgb * terrainMask.g; // ... + layers 2,3 by b,a
  // --- IF WATER ---
  vec2 v_waterTexCoords = v_tex0;
  v_waterTexCoords.x += u_moveFactor;
  vec2 v_ndc_pxl = (v_ndc.xy / v_ndc.w) / 2.0 + 0.5;
  float moveFactorDist = u_moveFactor * 3.0f;
  vec2 distortion1 = (texture(tex6, vec2(v_textureCoords.x + moveFactorDist, v_textureCoords.y)).rg * 2.0 - 1.0) * waveStrength;
  vec2 distortion2 = (texture(tex6, vec2(-v_textureCoords.x + moveFactorDist, v_textureCoords.y + moveFactorDist)).rg * 2.0 - 1.0) * waveStrength;
  vec2 totalDistortion = distortion1 + distortion2;
  vec2 v_reflectTexCoords = vec2(v_ndc_pxl.x, 1.0 - v_ndc_pxl.y);
  v_reflectTexCoords += totalDistortion;
  v_reflectTexCoords = clamp(v_reflectTexCoords, 0.001, 0.999);
  vec4 reflectColour = texture(tex8, v_reflectTexCoords);
  color = mix(color, reflectColour, 0.8);
  vec3 viewVec = normalize(u_eye - v_worldPosition);
  mediump float fresnelFactor = abs(dot(viewVec, vec3(0.0, 0.0, 1.0)));
  fresnelFactor = pow(fresnelFactor, 0.5);
  color.a = max(1.0 - fresnelFactor, 0.4);
  vec2 v_refractTexCoords = vec2(v_ndc_pxl.x, v_ndc_pxl.y);
  v_refractTexCoords += totalDistortion;
  v_refractTexCoords = clamp(v_refractTexCoords, 0.001, 0.999);
  vec4 refractColour = texture(tex4, v_refractTexCoords);
  color = mix(color, refractColour, fresnelFactor);

  color *= v_color;

  // ---- snow cover ----
  float snowStrength = step(u_snowCoverAmount, v_worldNormal.z);
  vec4 snowTex = texture(tex3, v_snowTileUv);
  color.rgb = mix(color.rgb, snowTex.rgb * u_snowCoverColor.rgb,
                  snowStrength * snowTex.a * u_snowCoverColor.a);

  // ---- shadows (shadow-map, poisson) ----
  float shadow = 0.0;
  // --- IF SHADOWMAP ---
  vec2 poissonDisk[4] = vec2[](
    vec2( -0.94201624, -0.39906216 ),
    vec2( 0.94558609, -0.76890725 ),
    vec2( -0.094184101, -0.92938870 ),
    vec2( 0.34495938, 0.29387760 ));
  float visibility = 0.0;
  if (!(v_shadowProj.x > 1.0 || v_shadowProj.y > 1.0 || v_shadowProj.x < 0.0 || v_shadowProj.y < 0.0)) {
    for (int i = 0; i < 4; i++)
      visibility += 0.25 * texture(tex5, vec3(v_shadowProj.x + poissonDisk[i].x * 0.0005,
                                              v_shadowProj.y + poissonDisk[i].y * 0.0005,
                                              v_shadowProj.z));
    shadow = 0.3 * (1.0 - visibility);
  }
  shadow = max(shadow, 0.3 * pow((1.0 - diffuse), 24.0));   // fallback
  // --- ELSE (no shadow map) ---
  shadow = 0.3 * pow((1.0 - diffuse), 24.0);

  // ---- sun lighting + specular + envmap (material permutation) ----
  highp vec3 viewVec2 = normalize(v_worldPosition - u_eye);
  highp vec3 Out_Refl = reflect(viewVec2, v_worldNormal);
  vec2 ReflPos = normalize(Out_Refl.xy) * (Out_Refl.z * 0.5 + 0.5);
  ReflPos = (ReflPos * vec2(0.5, 0.5)) + vec2(0.5, 0.5);
  vec3 envmap = texture(tex2, vec2(ReflPos.x, ReflPos.y)).rgb;
  highp vec3 lightDir   = normalize(u_sunPos - v_worldPosition);
  highp vec3 viewDir    = normalize(u_eye - v_worldPosition);
  highp vec3 halfwayDir = normalize(lightDir + viewDir);
  vec3 u_sunLightingColor = vec3(0.2, 0.2, 0.2);
  float spec = pow(max(dot(normalize(v_worldNormal), halfwayDir), 0.0), 50.0);
  vec3 specular = u_sunLightingColor * spec * 0.4;
  highp float diffuseCoef = max(dot(normalize(v_worldNormal), lightDir), 0.0);
  vec3 diffuseVec = diffuseCoef * diffuse * u_sunLightingColor;
  highp float fresnelFactor2 = 1.0 - abs(dot(-viewVec2, v_worldNormal));
  fresnelFactor2 = pow(fresnelFactor2, 3.5);
  fresnelFactor2 = clamp(fresnelFactor2, 0.15, 0.7);
  envmap.rgb = pow(envmap.rgb, vec3(0.8));
  envmap.rgb = envmap.rgb * fresnelFactor2 * u_envMapCoef;
  color.rgb += (diffuseVec + specular + envmap.rgb);

  // ---- emission ----
  if (u_pbrModelEmissionStrength <= 0.0) {
     vec4 emissiveColor = texture(tex1, v_tex0);
     color.rgb += emissiveColor.rgb * emissiveColor.a * u_emissiveSaturation;
  } else {
     float emissionStrength = texture(tex1, v_tex0).r * u_pbrModelEmissionStrength;
     vec3 emissionColor = emissionStrength * u_pbrModelEmissionColorAlpha.rgb
                        * mix(vec3(1.0), color.rgb, u_pbrModelEmissionColorAlpha.a);
     color.rgb += emissionColor;
  }

  // ---- fog / gamma / lightmap ----
  color.rgb = mix(color.rgb, u_fogColor.rgb, v_fog);
  color.rgb = pow(color.rgb, vec3(u_localInvGamma));
  vec4 lightmapColor = texture(tex1, v_tex1);
  color.rgb = mix(color.rgb, lightmapColor.rgb * texture(tex0, v_tex0).rgb * 3.0, lightmapColor.a);
  color.rgb *= u_lightmapMulti;
  color.rgb = color.rgb * (1.0 - shadow);

  DoAlphaTest(color.a);
  // --- IF DEBUG ---
  if (u_debugColor.a > 2.5) color.rgb = vec3(0.25, 0.11, 0.08);
  else if (u_debugColor.a > 1.5) color.rgb = normalize(v_eyeNormal) * 0.5 + 0.5;
  else color.rgb = mix(color.rgb, u_debugColor.rgb, u_debugColor.a);
  FRAGCOLOR(color);
}
