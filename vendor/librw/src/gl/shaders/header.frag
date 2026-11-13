#ifdef USE_UBOS
layout(std140) uniform State
{
	vec2 u_alphaRef;
	vec4  u_fogData;
	vec4  u_fogColor;
};
#else
uniform vec4 u_alphaRef;

uniform vec4  u_fogData;
uniform vec4  u_fogColor;
#endif

#define u_fogStart (u_fogData.x)
#define u_fogEnd (u_fogData.y)
#define u_fogRange (u_fogData.z)
#define u_fogDisable (u_fogData.w)

// --- Shaders For BLAYZ RUSSIA PC: PBR + Sun Shadow + Sky SH + ACES ---
uniform vec4 u_surfProps;	// x: amb, y: spec, z: diff, w: +1 prelit, +2 unlit
uniform vec4 u_eye;		// xyz: camera world position
uniform vec4 u_sunDir;		// xyz: direction TO the sun
uniform vec4 u_sunMaterial;	// rgb: directional timecycle colour, a: shadow strength
uniform vec4 u_atmosphere;	// x: enabled, y: clouds, z: fogginess, w: dayNightBalance
uniform vec4 u_skyAmbient;	// rgb: sky ambient colour, a: ACES whitePoint
uniform vec4 u_localInvGamma;	// x: gamma exponent
uniform mat4 u_shadowMatrix;	// directional sun lightProj * lightView
uniform vec4 u_shadowParams;	// x: strength, y: 1/mapSize, z: normalBias, w: fadeRadius
uniform vec4 u_sceneLightPosRad[8]; // xyz: world pos, w: radius
uniform vec4 u_sceneLightColor[8];  // rgb: linear intensity
uniform vec4 u_sceneLightDir[8];    // xyz: cone dir, w: >0.5 for directional spotlight
uniform vec4 u_colorGrade[3];       // 3x4 RGB grading matrix from data/colorcycle.dat
// x > 0.5 while the paraboloid capture pass draws the scene (0x42b527).
uniform vec4 u_paraboloid;
uniform sampler2D tex3;		// directional sun shadow depth map

#define BR_PI 3.14159265
#define BR_INV_PI 0.318309886
#define BR_INV_MIN_IOR 50.0
#define BR_DEFAULT_REFLECTANCE 0.5
#define BR_REFLECTANCE_MAX_AMP 0.16
#define BR_MIN_PBR_ALBEDO 0.04

float br_pow2(float v) { return v * v; }
float br_pow4(float v) { float v2 = v * v; return v2 * v2; }
float br_pow5(float v) { float v2 = v * v; return v2 * v2 * v; }
float br_saturate(float v) { return clamp(v, 0.0, 1.0); }
vec3 br_saturate3(vec3 v) { return clamp(v, 0.0, 1.0); }
float br_luminance(vec3 col) { return dot(col, vec3(0.299, 0.587, 0.114)); }

// ColorCorrectionCommon.glsl: exact sRGB -> linear transfer
vec4 BR_SRGBtoLINEAR(vec4 srgbIn)
{
	vec3 c = max(srgbIn.xyz, vec3(0.0));
	vec3 bLess = step(vec3(0.04045), c);
	vec3 linOut = mix(c / vec3(12.92), pow((c + vec3(0.055)) / vec3(1.055), vec3(2.4)), bLess);
	return vec4(linOut, srgbIn.w);
}

vec3 BR_SRGBtoLINEAR(vec3 srgbIn)
{
	return BR_SRGBtoLINEAR(vec4(srgbIn, 1.0)).rgb;
}

// ColorCorrectionCommon.glsl: Stephen Hill RRT+ODT ACES with whitePoint normalization
vec3 BR_Aces(vec3 color, float whitePoint)
{
	const float exposure_bias = 1.8;
	const float A = 0.0245786;
	const float B = 0.000090537;
	const float C = 0.983729;
	const float D = 0.432951;
	const float E = 0.238081;

	vec3 c = max(color, vec3(0.0)) * exposure_bias;
	vec3 rrt = vec3(
		dot(c, vec3(0.59719, 0.35458, 0.04823)),
		dot(c, vec3(0.07600, 0.90834, 0.01566)),
		dot(c, vec3(0.02840, 0.13383, 0.83777)));
	vec3 ct = max((rrt * (rrt + A) - B) / (rrt * (C * rrt + D) + E), vec3(0.0));
	vec3 odt = vec3(
		dot(ct, vec3( 1.60475, -0.53108, -0.07367)),
		dot(ct, vec3(-0.10208,  1.10813, -0.00605)),
		dot(ct, vec3(-0.00327, -0.07276,  1.07602)));

	float wp = max(whitePoint, 1.0) * exposure_bias;
	float wt = max((wp * (wp + A) - B) / (wp * (C * wp + D) + E), 1.0e-4);
	return max(odt / wt, vec3(0.0));
}

vec3 BR_TonemapAces(vec3 hdrLinear, float whitePoint, float gamma)
{
	vec3 mapped = BR_Aces(hdrLinear, whitePoint);
	return pow(max(mapped, vec3(0.0)), vec3((1.0 / max(gamma, 0.1)) * u_localInvGamma.x));
}

vec3 BR_ApplyMobileColorGrade(vec3 c)
{
	// u_colorGrade is the 3x4 RGB grading matrix sampled from data/colorcycle.dat
	// for the current hour/weather. It used to be uploaded and then ignored, so
	// custom weather colour cycles had no effect on the mobile pipeline.
	vec3 g = clamp(c, 0.0, 1.0);
	vec3 graded = vec3(dot(g, u_colorGrade[0].xyz) + u_colorGrade[0].w,
	                   dot(g, u_colorGrade[1].xyz) + u_colorGrade[1].w,
	                   dot(g, u_colorGrade[2].xyz) + u_colorGrade[2].w);
	return pow(clamp(graded, 0.0, 1.0), vec3(u_localInvGamma.x));
}

float AtmosphereVisibility(vec3 worldPos, float oldVisibility)
{
	if(u_atmosphere.x < 0.5 || u_fogDisable > 0.5 || u_fogEnd <= u_fogStart)
		return oldVisibility;
	float d = length(worldPos - u_eye.xyz);
	float span = max(u_fogEnd - u_fogStart, 1.0);
	float t = clamp((d - u_fogStart) / span, 0.0, 1.0);
	if(u_atmosphere.x >= 1.5)
		return 1.0 - t;
	float weather = clamp(0.7*clamp(u_atmosphere.y, 0.0, 1.0) +
	                      0.6*clamp(u_atmosphere.z, 0.0, 1.0), 0.0, 1.0);
	float density = 1.0 + 1.2*weather;
	float curved = (1.0 - exp(-density*t)) / (1.0 - exp(-density));
	float aboveEye = clamp((worldPos.z - u_eye.z) / span, 0.0, 1.0);
	float weight = 0.6*(1.0 - 0.35*aboveEye);
	return 1.0 - mix(t, curved, weight);
}

vec3 ResolveWorldNormal(vec3 worldPos, vec3 worldNormal, out float hasVertNormal)
{
	float normalLength2 = dot(worldNormal, worldNormal);
	if(normalLength2 > 1.0e-6){
		hasVertNormal = 1.0;
		return worldNormal * inversesqrt(normalLength2);
	}
	hasVertNormal = 0.0;
#ifndef GL2
	vec3 dx = dFdx(worldPos);
	vec3 dy = dFdy(worldPos);
	vec3 ng = cross(dx, dy);
	float ng2 = dot(ng, ng);
	if(ng2 > 1.0e-12){
		vec3 N = ng * inversesqrt(ng2);
		if(dot(N, u_eye.xyz - worldPos) < 0.0)
			N = -N;
		return N;
	}
#endif
	return vec3(0.0, 0.0, 1.0);
}

// 9-tap rotated Poisson PCF directional sun shadow lookup on desktop;
// 4-tap rotated Poisson PCF + planar self-shadow fallback on mobile (u_atmosphere.x >= 1.5).
float SampleSunShadow(vec3 worldPos, vec3 N)
{
	float sunLen2 = dot(u_sunDir.xyz, u_sunDir.xyz);
	vec3 L = sunLen2 > 1.0e-6 ? u_sunDir.xyz * inversesqrt(sunLen2) : vec3(0.0, 0.0, 1.0);
	float ndotl = clamp(dot(N, L), 0.0, 1.0);
	float slope = 1.0 - ndotl;

	// Terminator correction (mesh_shadow_* in the client): surfaces that face
	// away from the sun still read as sunlit in the shadow map, so the client
	// darkens them by strength*0.3*pow(1-diffuse, 24) on top of the map. Without
	// it the terminator of every curved surface (car bodies, peds, rounded
	// facades) pops from lit to shadowed along a hard line. Mobile carried it
	// inside planarVis; it is one shared term for both paths now so the
	// desktop branch gets the same falloff.
	float terminator = 0.3 * clamp(u_shadowParams.x, 0.0, 1.0) * pow((1.0 - ndotl), 24.0);

	if(u_atmosphere.x >= 1.5){
		float s2 = slope * slope;
		float s4 = s2 * s2;
		// planarVis is the mobile stand-in used when the map cannot answer
		// (outside the cascade, degenerate depth). The old constant 0.30 times
		// the same s^24 term the shared `terminator` computes is folded in here
		// through `terminator`, so it is not applied twice.
		float planarVis = 1.0;
		if(u_shadowParams.x <= 0.001)
			return max(1.0 - terminator, 0.0);
		vec3 mBiased = worldPos + N * (u_shadowParams.z * (0.45 + 1.15 * slope))
		                        + L * (u_shadowParams.z * 0.35);
		vec4 mSc = u_shadowMatrix * vec4(mBiased, 1.0);
		vec3 mProj = (mSc.xyz / max(mSc.w, 1.0e-5)) * 0.5 + 0.5;
		vec2 mEdge = abs(mProj.xy * 2.0 - 1.0);
		float mMaxEdge = max(mEdge.x, mEdge.y);
		if(mMaxEdge >= 0.992 || mProj.z <= 0.0005 || mProj.z >= 0.9995)
			return max(planarVis - terminator, 0.0);
		float mFade = smoothstep(0.992, 0.82, mMaxEdge);
		float mRad = max(u_shadowParams.y * 1.10, 0.0005);
		float mZRef = mProj.z - max(0.0018 * s4, 0.0001);
		float mVis = 0.0;
		mVis += (mZRef <= texture(tex3, mProj.xy + vec2(-0.94201624, -0.39906216) * mRad).r) ? 1.0 : 0.0;
		mVis += (mZRef <= texture(tex3, mProj.xy + vec2( 0.94558609, -0.76890725) * mRad).r) ? 1.0 : 0.0;
		mVis += (mZRef <= texture(tex3, mProj.xy + vec2(-0.09418410,  0.92938870) * mRad).r) ? 1.0 : 0.0;
		mVis += (mZRef <= texture(tex3, mProj.xy + vec2( 0.34495938,  0.29387760) * mRad).r) ? 1.0 : 0.0;
		mVis *= 0.25;
		float mapShadowVis = 1.0 - clamp(u_shadowParams.x, 0.0, 1.0) * (1.0 - mVis);
		return max(mix(planarVis, min(planarVis, mapShadowVis), mFade) - terminator, 0.0);
	}

	if(u_shadowParams.x <= 0.001)
		return 1.0;
	vec3 biasedPos = worldPos + N * (u_shadowParams.z * (0.45 + 1.15 * slope))
	                          + L * (u_shadowParams.z * 0.35);
	vec4 sc = u_shadowMatrix * vec4(biasedPos, 1.0);
	vec3 proj = (sc.xyz / max(sc.w, 1.0e-5)) * 0.5 + 0.5;
	vec2 edge = abs(proj.xy * 2.0 - 1.0);
	float maxEdge = max(edge.x, edge.y);
	if(maxEdge >= 0.995 || proj.z <= 0.0005 || proj.z >= 0.9995)
		return max(1.0 - terminator, 0.0);
	float cascadeFade = smoothstep(0.995, 0.80, maxEdge);

	float ang = 6.2831853 * fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
	float cs = cos(ang);
	float sn = sin(ang);
	mat2 rot = mat2(cs, sn, -sn, cs);
	float rad = u_shadowParams.y * 1.85;
	float zRef = proj.z - (0.00020 + 0.00060 * slope * slope);

	float vis = 0.0;
	vis += (zRef <= texture(tex3, proj.xy).r) ? 1.0 : 0.0;
	vis += (zRef <= texture(tex3, proj.xy + (rot * vec2(-0.94201624, -0.39906216)) * rad).r) ? 1.0 : 0.0;
	vis += (zRef <= texture(tex3, proj.xy + (rot * vec2( 0.94558609, -0.76890725)) * rad).r) ? 1.0 : 0.0;
	vis += (zRef <= texture(tex3, proj.xy + (rot * vec2(-0.09418410, -0.92938870)) * rad).r) ? 1.0 : 0.0;
	vis += (zRef <= texture(tex3, proj.xy + (rot * vec2( 0.34495938,  0.29387760)) * rad).r) ? 1.0 : 0.0;
	vis += (zRef <= texture(tex3, proj.xy + (rot * vec2(-0.91588581,  0.45771432)) * rad).r) ? 1.0 : 0.0;
	vis += (zRef <= texture(tex3, proj.xy + (rot * vec2(-0.38277543,  0.27676845)) * rad).r) ? 1.0 : 0.0;
	vis += (zRef <= texture(tex3, proj.xy + (rot * vec2( 0.97484398,  0.75648379)) * rad).r) ? 1.0 : 0.0;
	vis += (zRef <= texture(tex3, proj.xy + (rot * vec2( 0.44323325, -0.97511554)) * rad).r) ? 1.0 : 0.0;
	vis *= (1.0 / 9.0);
	return max(mix(1.0, vis, clamp(u_shadowParams.x, 0.0, 1.0) * cascadeFade) - terminator, 0.0);
}

// --- pbr/ModelsBRDF.glsl & pbr/RoughMip.glsl & pbr/LightMaterialGBufFS.glsl ---

// ModelsBRDF.glsl: PI division is included in sunDirectColor / sunAmbientColor (BR_INV_PI).
vec3 BR_DiffuseBurleyFixed(vec3 diffuseColor, float linearRoughness, float NoV, float NoL, float VoH)
{
	float energyFactor = (0.45 * linearRoughness - 0.475) * linearRoughness + 1.0;
	float FD90 = 2.0 * VoH * VoH * linearRoughness + 0.5;
	float FD90Inv = FD90 - 1.0;
	float FdV = 1.0 + FD90Inv * br_pow5(1.0 - NoV);
	float FdL = 1.0 + FD90Inv * br_pow5(1.0 - NoL);
	return diffuseColor * (FdV * FdL * energyFactor);
}

float BR_DistributionGGX(float ggxAlpha, float NoH)
{
	float alpha2 = ggxAlpha * ggxAlpha;
	float d = (NoH * alpha2 - NoH) * NoH + 1.0;
	return alpha2 / max(1.0e-7, d * d);
}

float BR_GeometrySmithCorrelated(float ggxAlpha, float NoV, float NoL)
{
	float a2 = br_pow2(ggxAlpha);
	float gv = NoL * sqrt(NoV * (NoV - NoV * a2) + a2);
	float gl = NoV * sqrt(NoL * (NoL - NoL * a2) + a2);
	return 0.5 / max(1.0e-6, gv + gl);
}

vec3 BR_FresnelSchlick(vec3 specularColor, float VoH)
{
	float Fc = br_pow5(1.0 - VoH);
	return vec3(br_saturate(BR_INV_MIN_IOR * specularColor.g) * Fc) + (1.0 - Fc) * specularColor;
}

float BR_FoliageBRDFSSS(float NoL, vec3 view, vec3 lightDir)
{
	float LdotNBack = -NoL;
	float diffuseWrap = 0.6;
	float backDiff = br_saturate(LdotNBack * (diffuseWrap * diffuseWrap) + (1.0 - diffuseWrap) * diffuseWrap);
	float EdotL = br_saturate(dot(view, -lightDir));
	float PowEdotL = br_pow4(EdotL) * (9.0 / (2.0 * BR_PI));
	return mix(backDiff, PowEdotL, 0.5);
}

vec3 BR_StandardBRDF(
	float NoV, float NoL, vec3 baseDiffuseColor, float ggxAlpha, float linearRoughness,
	vec3 specularColor, float specularStrength, vec3 lightDir, vec3 view, vec3 normal)
{
	vec3 halfSum = view + lightDir;
	vec3 H = halfSum * inversesqrt(max(dot(halfSum, halfSum), 1.0e-6));
	float NoH = br_saturate(dot(normal, H));
	float VoH = br_saturate(dot(view, H));
	vec3 diffuse = BR_DiffuseBurleyFixed(baseDiffuseColor, linearRoughness, NoV, NoL, VoH);
	float specDG = BR_DistributionGGX(ggxAlpha, NoH) * BR_GeometrySmithCorrelated(ggxAlpha, NoV, NoL) * specularStrength;
	vec3 F = BR_FresnelSchlick(specularColor, VoH);
	return (diffuse + F * specDG) * NoL;
}

vec3 BR_GetEnvBRDFApprox(vec3 specularColor, float linearRoughness, float NoV)
{
	vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
	vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
	vec4 r = linearRoughness * c0 + c1;
	float a004 = min(r.x * r.x, exp2(-9.28 * NoV)) * r.x + r.y;
	vec2 AB = vec2(-1.04, 1.04) * a004 + r.zw;
	return specularColor * AB.x + vec3(AB.y * br_saturate(BR_INV_MIN_IOR * specularColor.g));
}

float BR_ComputeSpecOcclusion(float saturatedNdotV, float AO, float ggxAlpha)
{
	return br_saturate(pow(max(saturatedNdotV + AO, 1.0e-4), exp2(-16.0 * ggxAlpha - 1.0)) - 1.0 + AO);
}

vec3 BR_GetRoughReflectionVec(vec3 R, vec3 normal, float ggxAlpha)
{
	float k = max(1.0 - ggxAlpha, 0.0);
	return mix(normal, R, k * (sqrt(k) + ggxAlpha));
}

float BR_DecodeAlbedoAO(vec3 albedo)
{
	return br_saturate(br_luminance(albedo) * (1.0 / BR_MIN_PBR_ALBEDO)) * 0.9 + 0.1;
}

vec3 BR_GetSkySHDiffuse(vec3 normal, vec3 sunAmbientColor)
{
	float normBlend = max(normal.z, 0.0) * 0.5 + 0.5;
	float peak = max(sunAmbientColor.x, max(sunAmbientColor.y, sunAmbientColor.z));
	vec3 sunSkyColor = mix(vec3(peak), sunAmbientColor, normBlend);
	return sunSkyColor * normBlend;
}

vec3 BR_DecodeSpecularColor(float reflectance, float metalness, vec3 albedo)
{
	float R0Dielectric = BR_REFLECTANCE_MAX_AMP * br_pow2(reflectance);
	return mix(vec3(R0Dielectric), albedo * br_saturate(reflectance * 50.0), metalness);
}

// Scene sun direct & sky SH colours with 1/PI pre-baked as required by ModelsBRDF.glsl
void BR_GetSceneLightColors(vec3 L, out vec3 sunDirectColor, out vec3 sunAmbientColor, out float dayFactor)
{
	dayFactor = smoothstep(-0.04, 0.20, L.z);
	float highSun = smoothstep(0.02, 0.55, L.z);
	float cloud = clamp(u_atmosphere.y, 0.0, 1.0);
	float clearSun = dayFactor * (1.0 - 0.50 * cloud);

	// Golden warm sunrise/sunset transitioning to crisp neutral-warm noon sun, divided by PI
	vec3 goldenSun = vec3(2.55, 1.88, 1.26) * BR_INV_PI;
	vec3 noonSun   = vec3(2.62, 2.48, 2.26) * BR_INV_PI;
	vec3 physSun   = mix(goldenSun, noonSun, highSun);
	vec3 tcSunLin  = mix(vec3(1.0), clamp(u_sunMaterial.rgb, vec3(0.35), vec3(1.15)), 0.25);
	sunDirectColor = physSun * tcSunLin * clearSun;

	// Hemispherical Sky SH irradiance divided by PI (readable cool moonlight at night)
	vec3 duskAmb  = vec3(0.62, 0.68, 0.88) * BR_INV_PI;
	vec3 noonAmb  = vec3(0.86, 0.98, 1.20) * BR_INV_PI;
	vec3 nightAmb = vec3(0.46, 0.55, 0.78) * BR_INV_PI;
	vec3 dayAmb   = mix(duskAmb, noonAmb, highSun);
	sunAmbientColor = mix(nightAmb, dayAmb, dayFactor) * (1.0 - 0.18 * cloud);
}

vec3 BR_EvalAnalyticalSkyEnv(vec3 roughR, vec3 L, vec3 sunDirectColor, vec3 sunAmbientColor)
{
	vec3 r = roughR * inversesqrt(max(dot(roughR, roughR), 1.0e-6));
	float h = clamp(r.z * 0.5 + 0.5, 0.0, 1.0);
	vec3 groundCol = sunAmbientColor * vec3(0.45, 0.44, 0.42);
	vec3 horizonCol = sunAmbientColor * vec3(1.65, 1.72, 1.85);
	vec3 zenithCol = sunAmbientColor * vec3(0.95, 1.20, 1.68);
	vec3 env = mix(groundCol, mix(horizonCol, zenithCol, smoothstep(0.48, 0.95, h)), smoothstep(0.35, 0.58, h));
	float sunGlow = pow(max(dot(r, L), 0.0), 28.0) * smoothstep(-0.05, 0.20, r.z);
	env += sunDirectColor * (0.22 * sunGlow);
	return env;
}

// Unified LightMaterialGBufFS shading (exact match to pbr/LightMaterialGBufFS.glsl + ParaboloidEnvMap.glsl)
vec3 BR_ShadeMaterialHDR(
	vec3 albedoLin,
	vec3 worldPos,
	vec3 N,
	float smoothness,
	float metalness,
	float reflectance,
	float ao,
	float isFoliage,
	float translucency,
	float emissionStrength,
	float emissionPart,
	vec3 customEnvLin,
	float useCustomEnv)
{
	float sunLen2 = dot(u_sunDir.xyz, u_sunDir.xyz);
	vec3 L = sunLen2 > 1.0e-6 ? u_sunDir.xyz * inversesqrt(sunLen2) : vec3(0.0, 0.0, 1.0);

	vec3 sunDirectColor;
	vec3 sunAmbientColor;
	float dayFactor;
	BR_GetSceneLightColors(L, sunDirectColor, sunAmbientColor, dayFactor);

	float shadowTerm = dayFactor > 0.001 ? SampleSunShadow(worldPos, N) : 1.0;

	vec3 pointToEye = u_eye.xyz - worldPos;
	float distSq = max(dot(pointToEye, pointToEye), 1.0e-6);
	vec3 view = pointToEye * inversesqrt(distSq);

	float NdotV = dot(N, view);
	float NoV = abs(NdotV) + 1.0e-5;
	float linearRoughness = clamp(1.0 - smoothness, 0.04, 1.0);
	float ggxAlpha = max(1.0e-4, linearRoughness * linearRoughness);

	float extractedAlbedoAO = BR_DecodeAlbedoAO(albedoLin);
	vec3 diffuseColor = albedoLin * (1.0 - metalness);
	vec3 specularColor = BR_DecodeSpecularColor(reflectance, metalness, albedoLin);
	float enviAOForSpecular = min(ao, extractedAlbedoAO * 0.5 + 0.5);
	specularColor *= BR_ComputeSpecOcclusion(NoV, enviAOForSpecular, ggxAlpha);

	vec3 result = vec3(0.0);
	float NoL = dot(N, L);
	if(shadowTerm > 0.001 && NoL > 0.0){
		result = BR_StandardBRDF(
			NoV, NoL, diffuseColor, ggxAlpha, linearRoughness,
			specularColor, extractedAlbedoAO, L, view, N) * shadowTerm * sunDirectColor;
	}

	// Subtle cool moonlight directional key at night so facades & vehicles retain 3D depth
	float nightFactor = 1.0 - dayFactor;
	if(nightFactor > 0.01){
		vec3 moonL = vec3(-0.30, -0.26, 0.918);
		float NoMoon = max(dot(N, moonL), 0.0);
		if(NoMoon > 0.0){
			vec3 moonColor = vec3(0.28, 0.34, 0.50) * BR_INV_PI * nightFactor;
			result += (diffuseColor * NoMoon + specularColor * (0.08 * pow(max(dot(N, normalize(moonL + view)), 0.0), 24.0))) * moonColor;
		}
	}

	// Dynamic per-pixel streetlamps, traffic lights & vehicle headlights from CPointLights::aLights
	for(int li = 0; li < 8; li++){
		float rad = u_sceneLightPosRad[li].w;
		if(rad <= 0.01)
			break;
		vec3 toLight = u_sceneLightPosRad[li].xyz - worldPos;
		float d2 = dot(toLight, toLight);
		float r2 = rad * rad;
		if(d2 >= r2)
			continue;
		float invD = inversesqrt(max(d2, 1.0e-4));
		vec3 Lp = toLight * invD;
		float x2 = d2 / r2;
		float atten = br_pow2(max(1.0 - x2, 0.0)) / (1.0 + 3.2 * x2);
		if(u_sceneLightDir[li].w > 0.5){
			float cosCone = dot(-Lp, u_sceneLightDir[li].xyz);
			atten *= smoothstep(0.30, 0.78, cosCone);
		}
		if(atten <= 0.0005)
			continue;
		float wrapDiff = max(dot(N, Lp) * 0.78 + 0.22, 0.0);
		vec3 Hp = Lp + view;
		Hp *= inversesqrt(max(dot(Hp, Hp), 1.0e-6));
		float specLobe = pow(max(dot(N, Hp), 0.0), mix(12.0, 88.0, smoothness)) * mix(0.06, 0.42, smoothness);
		vec3 lCol = u_sceneLightColor[li].rgb * (2.65 * BR_INV_PI);
		result += (diffuseColor * wrapDiff + specularColor * specLobe) * lCol * atten;
	}

	vec3 skyAmbient = BR_GetSkySHDiffuse(N, sunAmbientColor) * diffuseColor * ao;
	if(isFoliage > 0.5){
		vec3 transCol = diffuseColor * translucency * vec3(1.0, 0.8, 0.5);
		if(shadowTerm > 0.001){
			float backShading = BR_FoliageBRDFSSS(NoL, view, L);
			result += (backShading * ao * shadowTerm) * sunDirectColor * transCol;
		}
		skyAmbient += ao * BR_GetSkySHDiffuse(-N, sunAmbientColor) * transCol;
	}

	// ParaboloidEnvMap.glsl: INTERVAL_PARABOLOIDENVMAP is 0 for Building/Skin/Terrain/GL3Shader,
	// and active only when useCustomEnv > 0 (Vehicle.fs & MatFX reflective surfaces).
	vec3 enviBRDF = BR_GetEnvBRDFApprox(specularColor, linearRoughness, NoV);
	vec3 envReflection = customEnvLin * useCustomEnv * mix(0.60, 1.0, shadowTerm) * enviBRDF;

	result += skyAmbient + ao * envReflection;

	vec3 emissionColor = emissionStrength * albedoLin;
	result = result * (1.0 - emissionPart) + emissionColor;
	return max(result, vec3(0.0));
}

// Legacy helper kept for GL3Atmosphere=0 fallback compatibility.
vec3 SunMaterialLighting(vec3 worldPos, vec3 worldNormal, out float selfShade)
{
	float normalLength2 = dot(worldNormal, worldNormal);
	if(normalLength2 < 1.0e-6){
		selfShade = 0.0;
		return vec3(0.0);
	}
	vec3 N = worldNormal * inversesqrt(normalLength2);
	vec3 L = normalize(u_sunDir.xyz);
	float day = smoothstep(-0.05, 0.15, u_sunDir.z);
	float diffuse = max(dot(N, L), 0.0);
	float cloud = u_atmosphere.x > 0.5 ? clamp(u_atmosphere.y, 0.0, 1.0) : 0.0;
	float facing = smoothstep(0.0, mix(0.25, 0.40, cloud), diffuse);
	selfShade = mix(0.30, 0.18, cloud) * u_sunMaterial.a * day * (1.0 - facing);

	vec3 toEye = u_eye.xyz - worldPos;
	vec3 V = toEye * inversesqrt(max(dot(toEye, toEye), 1.0e-6));
	vec3 halfDir = L + V;
	vec3 H = halfDir * inversesqrt(max(dot(halfDir, halfDir), 1.0e-6));
	float specular = pow(max(dot(N, H), 0.0), 50.0) * facing;
	return 0.2 * u_sunMaterial.rgb * day * (diffuse + mix(0.4, 0.12, cloud)*specular);
}

vec3 BR_ShadeMobileWorldLinear(
	vec3 texSrgb, vec3 vertColor, vec3 worldPos, vec3 worldNormal, float isSkinDynamic)
{
	float hasVertNormal;
	vec3 N = ResolveWorldNormal(worldPos, worldNormal, hasVertNormal);
	float isFoliage = (isSkinDynamic < 0.5 && u_alphaRef.x > 0.01) ? 1.0 : 0.0;
	if(isFoliage > 0.5 && hasVertNormal < 0.5)
		N = normalize(mix(N, vec3(0.0, 0.0, 1.0), 0.70));

	float sunLen2 = dot(u_sunDir.xyz, u_sunDir.xyz);
	vec3 L = sunLen2 > 1.0e-6 ? u_sunDir.xyz * inversesqrt(sunLen2) : vec3(0.0, 0.0, 1.0);
	float day = smoothstep(-0.04, 0.20, L.z);
	float highSun = smoothstep(0.02, 0.55, L.z);
	float cloud = clamp(u_atmosphere.y, 0.0, 1.0);

	float shadowVis = day > 0.001 ? SampleSunShadow(worldPos, N) : 1.0;

	// The client's mobile shaders (mesh_shadow_*.vs / .fs) add the baked vertex
	// colour to the material ambient and multiply the texture by that sum:
	//     v_color = in_color + u_materialAmbient;
	//     color   = texture * v_color;
	// There is no separate sky-ambient colour term and no second multiplier
	// on the vertex colour. The port used to run the whole light model and then
	// multiply it by the baked vertex colour again, which darkened every
	// prelit surface a second time, and it let a cold sky term make up 82% of
	// the total light. That combination is what made the world flat, grey and
	// blue on device: grass lost half its saturation and pale walls came out
	// colder than they are in the texture.
	//
	// So: keep the vertex colour as the ambient/occlusion carrier the way the
	// client does, scale the sky tint down so it only *tints* rather than
	// dominates, and let the sun carry the contrast.
	vec3 baked = clamp(vertColor, vec3(0.0), vec3(1.0));
	float occlusion = dot(baked, vec3(0.299, 0.587, 0.114));

	// Sky tint: starts from the neutral day/night ambient and picks up only a
	// minority of the timecycle sky colour, so it can no longer push every
	// surface towards blue.
	vec3 daySkyAmb = mix(vec3(0.84, 0.85, 0.88), vec3(0.92, 0.94, 0.98), highSun);
	vec3 skyTint = mix(daySkyAmb, clamp(u_skyAmbient.rgb, vec3(0.30), vec3(1.10)), 0.12);
	vec3 nightAmb = vec3(0.34, 0.39, 0.52);
	float hemi = clamp(N.z * 0.5 + 0.5, 0.0, 1.0);
	// Ambient is now a *fill*: low enough that the sun decides the value, but
	// still brighter looking up than looking down.
	// Calibrated against the client screenshots on the sunlit parts of the
	// pavement, grass and a pale wall: ambient is only a thin fill (0.12) and
	// the sun carries the value. Sampling the *shaded* parts instead would have
	// picked 0.36 and left the world 40% too dark, because the client's
	// screenshots were also taken with shadows disabled.
	// Day and night need different fills: the sun carries the day, but at night
	// there is no sun term at all, so a day-sized 0.12 would leave everything
	// almost black (measured 0.04 luminance). Night keeps its own, larger fill.
	float ambScale = mix(mix(0.26, 0.34, hemi), mix(0.10, 0.15, hemi) * (1.0 - 0.18 * cloud), day);
	vec3 ambCol = mix(nightAmb, skyTint, day) * ambScale;

	// Ambient is carried by the baked vertex colour (ao), not by an extra
	// global multiplier -- that is where the client's contrast comes from.
	vec3 baseCol = texSrgb;
	vec3 ambientTerm = baseCol * ambCol * mix(0.30, 1.0, occlusion);

	float ndotl = max(dot(N, L), 0.0);
	if(isFoliage > 0.5)
		ndotl = max(ndotl, max(dot(-N, L), 0.0) * 0.42);

	// Sun now carries the contrast. Sunrise/sunset keep the warm low-sun colour,
	// noon is close to neutral, and the timecycle sun colour only nudges it.
	vec3 goldenSun = vec3(1.06, 0.94, 0.76);
	vec3 noonSun   = vec3(0.98, 0.97, 0.95);
	vec3 sunLightCol = mix(goldenSun, noonSun, highSun);
	sunLightCol = mix(sunLightCol, sunLightCol * clamp(u_sunMaterial.rgb, vec3(0.60), vec3(1.15)), 0.25);

	vec3 diffuseVec = ndotl * baseCol * sunLightCol * (day * (1.0 - 0.35 * cloud) * 0.80);

	vec3 toEye = u_eye.xyz - worldPos;
	vec3 V = toEye * inversesqrt(max(dot(toEye, toEye), 1.0e-6));
	vec3 H = L + V;
	H *= inversesqrt(max(dot(H, H), 1.0e-6));
	float specAmt = pow(max(dot(N, H), 0.0), 50.0) * (isSkinDynamic > 0.5 ? 0.40 : 0.15) * day * (1.0 - 0.45 * cloud);
	vec3 specular = sunLightCol * specAmt;

	vec3 lit = (ambientTerm + diffuseVec) * shadowVis + specular * shadowVis;

	// Static world geometry uses 0 per-pixel pointlights on mobile; dynamic skins catch up to 2 nearby beams
	if(isSkinDynamic > 0.5){
		for(int li = 0; li < 2; li++){
			float rad = u_sceneLightPosRad[li].w;
			if(rad <= 0.01)
				break;
			vec3 toLight = u_sceneLightPosRad[li].xyz - worldPos;
			float d2 = dot(toLight, toLight);
			float r2 = rad * rad;
			if(d2 >= r2)
				continue;
			vec3 Lp = toLight * inversesqrt(max(d2, 1.0e-4));
			float x2 = d2 / r2;
			float atten = br_pow2(max(1.0 - x2, 0.0)) / (1.0 + 2.8 * x2);
			if(u_sceneLightDir[li].w > 0.5)
				atten *= smoothstep(0.30, 0.78, dot(-Lp, u_sceneLightDir[li].xyz));
			float wrapDiff = max(dot(N, Lp) * 0.75 + 0.25, 0.0);
			lit += baseCol * u_sceneLightColor[li].rgb * (wrapDiff * atten * 0.85);
		}
	}
	return max(lit, vec3(0.0));
}

// Building.fs (smoothness=0.20) & Skin.fs / GL3Shader.fs (smoothness=0.40)
vec3 ShadeWorldSurfaceEx(
	vec3 texSrgb, vec3 vertColor, vec3 worldPos, vec3 worldNormal, float visibility, float isSkinDynamic)
{
	if(u_atmosphere.x >= 1.5){
		if(u_surfProps.w >= 1.5){
			vec3 mUnlit = mix(u_fogColor.rgb, clamp(texSrgb * vertColor, 0.0, 1.0), visibility);
			return BR_ApplyMobileColorGrade(mUnlit);
		}
		vec3 mLit = BR_ShadeMobileWorldLinear(texSrgb, vertColor, worldPos, worldNormal, isSkinDynamic);
		return BR_ApplyMobileColorGrade(mix(u_fogColor.rgb, mLit, visibility));
	}

	if(u_surfProps.w >= 1.5){
		vec3 unlit = clamp(texSrgb * vertColor, 0.0, 1.0);
		if(u_atmosphere.x < 0.5)
			return pow(unlit, vec3(u_localInvGamma.x));
		vec3 emissiveHdr = BR_SRGBtoLINEAR(vec4(unlit, 1.0)).rgb * 1.45;
		float wpUnlit = max(u_skyAmbient.a, 2.5);
		return mix(u_fogColor.rgb, BR_TonemapAces(emissiveHdr, wpUnlit, 2.2), visibility);
	}

	if(u_atmosphere.x < 0.5){
		float selfShade;
		vec3 legacy = (texSrgb * vertColor) + SunMaterialLighting(worldPos, worldNormal, selfShade);
		legacy *= 1.0 - selfShade;
		legacy = mix(u_fogColor.rgb, legacy, visibility);
		return pow(max(legacy, vec3(0.0)), vec3(u_localInvGamma.x));
	}

	float hasVertNormal;
	vec3 N = ResolveWorldNormal(worldPos, worldNormal, hasVertNormal);
	vec3 albedoLin = BR_SRGBtoLINEAR(vec4(texSrgb, 1.0)).rgb * max(vertColor, vec3(0.0));

	float isFoliage = (isSkinDynamic < 0.5 && u_alphaRef.x > 0.01) ? 1.0 : 0.0;
	if(isFoliage > 0.5 && hasVertNormal < 0.5){
		// Smooth hemispherical crown normal so crossed billboard tree cards have no X-seam
		N = normalize(mix(N, vec3(0.0, 0.0, 1.0), 0.70));
	}

	// Building.fs / Terrain.fs: smoothness = 0.20; Skin.fs / GL3Shader.fs: smoothness = 0.40
	float smoothness = isSkinDynamic > 0.5 ? 0.40 : 0.20;

	vec3 hdr = BR_ShadeMaterialHDR(
		albedoLin, worldPos, N,
		smoothness, 0.0, BR_DEFAULT_REFLECTANCE, 1.0,
		isFoliage, isFoliage * 0.42,
		0.0, 0.0,
		vec3(0.0), 0.0);

	float whitePoint = max(u_skyAmbient.a, 2.5);
	vec3 ldr = BR_TonemapAces(hdr, whitePoint, 2.2);
	return mix(u_fogColor.rgb, ldr, visibility);
}

#ifndef GL2
out vec4 fragColor;
#endif

void DoAlphaTest(float a)
{
#ifndef NO_ALPHATEST
	if(a < u_alphaRef.x || a >= u_alphaRef.y)
		discard;
#endif
}
