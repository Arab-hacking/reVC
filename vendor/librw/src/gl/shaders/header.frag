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
uniform vec4 u_eye;		// xyz: camera world position
uniform vec4 u_sunDir;		// xyz: direction TO the sun
uniform vec4 u_sunMaterial;	// rgb: directional timecycle colour, a: shadow strength
uniform vec4 u_atmosphere;	// x: enabled, y: clouds, z: fogginess, w: dayNightBalance
uniform vec4 u_skyAmbient;	// rgb: sky ambient colour, a: ACES whitePoint
uniform vec4 u_localInvGamma;	// x: gamma exponent
uniform mat4 u_shadowMatrix;	// directional sun lightProj * lightView
uniform vec4 u_shadowParams;	// x: strength, y: 1/mapSize, z: normalBias, w: fadeRadius
uniform sampler2D tex3;		// directional sun shadow depth map

#define BR_PI 3.14159265
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

float AtmosphereVisibility(vec3 worldPos, float oldVisibility)
{
	if(u_atmosphere.x < 0.5 || u_fogDisable > 0.5 || u_fogEnd <= u_fogStart)
		return oldVisibility;
	float d = length(worldPos - u_eye.xyz);
	float span = max(u_fogEnd - u_fogStart, 1.0);
	float t = clamp((d - u_fogStart) / span, 0.0, 1.0);
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

// 9-tap rotated Poisson PCF directional sun shadow lookup (1 = lit, 0 = occluded).
float SampleSunShadow(vec3 worldPos, vec3 N)
{
	if(u_shadowParams.x <= 0.001)
		return 1.0;
	float sunLen2 = dot(u_sunDir.xyz, u_sunDir.xyz);
	vec3 L = sunLen2 > 1.0e-6 ? u_sunDir.xyz * inversesqrt(sunLen2) : vec3(0.0, 0.0, 1.0);
	float ndotl = clamp(dot(N, L), 0.0, 1.0);
	float slope = 1.0 - ndotl;
	vec3 biasedPos = worldPos + N * (u_shadowParams.z * (0.45 + 1.15 * slope))
	                          + L * (u_shadowParams.z * 0.35);
	vec4 sc = u_shadowMatrix * vec4(biasedPos, 1.0);
	vec3 proj = (sc.xyz / max(sc.w, 1.0e-5)) * 0.5 + 0.5;
	vec2 edge = abs(proj.xy * 2.0 - 1.0);
	float maxEdge = max(edge.x, edge.y);
	if(maxEdge >= 0.995 || proj.z <= 0.0005 || proj.z >= 0.9995)
		return 1.0;
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
	return mix(1.0, vis, clamp(u_shadowParams.x, 0.0, 1.0) * cascadeFade);
}

// --- pbr/ModelsBRDF.glsl & pbr/RoughMip.glsl & pbr/LightMaterialGBufFS.glsl ---

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

void BR_GetSceneLightColors(vec3 L, out vec3 sunDirectColor, out vec3 sunAmbientColor, out float dayFactor)
{
	dayFactor = smoothstep(-0.04, 0.18, L.z);
	float cloud = clamp(u_atmosphere.y, 0.0, 1.0);
	float clearSun = dayFactor * (1.0 - 0.50 * cloud);
	vec3 tcSun = max(u_sunMaterial.rgb, vec3(0.55, 0.50, 0.44) * dayFactor);
	sunDirectColor = mix(tcSun, vec3(1.02, 0.96, 0.86), 0.48) * (2.35 * clearSun);

	vec3 tcSky = max(u_skyAmbient.rgb, vec3(0.10, 0.13, 0.20));
	vec3 dayAmb = mix(vec3(0.82, 0.94, 1.14), tcSky * 1.55, 0.48);
	vec3 nightAmb = mix(vec3(0.13, 0.16, 0.24), tcSky * 0.75, 0.45);
	sunAmbientColor = mix(nightAmb, dayAmb, dayFactor) * (1.0 - 0.18 * cloud);
}

vec3 BR_EvalAnalyticalSkyEnv(vec3 roughR, vec3 L, vec3 sunDirectColor, vec3 sunAmbientColor)
{
	vec3 r = roughR * inversesqrt(max(dot(roughR, roughR), 1.0e-6));
	float h = clamp(r.z * 0.5 + 0.5, 0.0, 1.0);
	vec3 groundCol = sunAmbientColor * vec3(0.30, 0.29, 0.28);
	vec3 horizonCol = sunAmbientColor * vec3(0.88, 0.90, 0.95);
	vec3 zenithCol = sunAmbientColor * vec3(0.55, 0.68, 0.92);
	vec3 env = mix(groundCol, mix(horizonCol, zenithCol, smoothstep(0.48, 0.95, h)), smoothstep(0.35, 0.58, h));
	float sunGlow = pow(max(dot(r, L), 0.0), 24.0) * smoothstep(-0.05, 0.20, r.z);
	env += sunDirectColor * (0.12 * sunGlow);
	return env;
}

// Unified LightMaterialGBufFS shading (used by Building.fs, GL3Shader.fs, Skin.fs, Vehicle.fs, MatFX)
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
	vec3 reflectionVec = 2.0 * NdotV * N - view;

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

	vec3 skyAmbient = BR_GetSkySHDiffuse(N, sunAmbientColor) * diffuseColor * ao;
	if(isFoliage > 0.5){
		vec3 transCol = diffuseColor * translucency * vec3(1.0, 0.8, 0.5);
		if(shadowTerm > 0.001){
			float backShading = BR_FoliageBRDFSSS(NoL, view, L);
			result += (backShading * ao * shadowTerm) * sunDirectColor * transCol;
		}
		skyAmbient += ao * BR_GetSkySHDiffuse(-N, sunAmbientColor) * transCol;
	}

	vec3 roughR = BR_GetRoughReflectionVec(reflectionVec, N, ggxAlpha);
	vec3 enviBRDF = BR_GetEnvBRDFApprox(specularColor, linearRoughness, NoV);
	vec3 analyticEnv = BR_EvalAnalyticalSkyEnv(roughR, L, sunDirectColor, sunAmbientColor);
	vec3 envSample = mix(analyticEnv, customEnvLin, useCustomEnv);
	envSample *= mix(0.62, 1.0, shadowTerm);

	result += skyAmbient + ao * envSample * enviBRDF;

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

// Compatibility wrapper for world/prop/ped shaders using linear BR PBR + ACES.
vec3 ShadeWorldSurfaceEx(
	vec3 texSrgb, vec3 vertColor, vec3 worldPos, vec3 worldNormal, float visibility, float isSkinDynamic)
{
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
	float smoothness = isSkinDynamic > 0.5 ? 0.40 : mix(0.20, 0.38, hasVertNormal * (1.0 - isFoliage));
	float dayNight = clamp(u_atmosphere.w, 0.0, 1.0);
	float bakedNightPeak = max(vertColor.r, max(vertColor.g, vertColor.b));
	float nightEmit = (isSkinDynamic < 0.5 && isFoliage < 0.5)
		? (dayNight * smoothstep(1.02, 1.25, bakedNightPeak) * 1.35)
		: 0.0;

	vec3 hdr = BR_ShadeMaterialHDR(
		albedoLin, worldPos, N,
		smoothness, 0.0, BR_DEFAULT_REFLECTANCE, 1.0,
		isFoliage, isFoliage * 0.38,
		nightEmit, 0.0,
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
