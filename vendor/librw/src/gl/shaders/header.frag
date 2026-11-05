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

// --- Black Russia client material parity + GL3 sun shadow & sky lighting ---
uniform vec4 u_eye;		// xyz: camera world position
uniform vec4 u_sunDir;		// xyz: direction TO the sun
uniform vec4 u_sunMaterial;	// rgb: directional timecycle colour, a: shadow strength
uniform vec4 u_atmosphere;	// x: enabled, y: clouds, z: fogginess (0..1)
uniform vec4 u_skyAmbient;	// rgb: sky hemisphere tint, a: ground bounce weight
uniform vec4 u_localInvGamma;	// x: gamma exponent
uniform mat4 u_shadowMatrix;	// directional sun lightProj * lightView
uniform vec4 u_shadowParams;	// x: strength, y: 1/mapSize, z: normalBias, w: fadeRadius
uniform sampler2D tex3;		// directional sun shadow depth map

// Per-fragment aerial perspective, bounded by the timecycle's fog start and
// VC's existing far clip.
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
	if(normalLength2 > 1.0e-8){
		hasVertNormal = 1.0;
		return worldNormal * inversesqrt(normalLength2);
	}
	hasVertNormal = 0.0;
#ifndef GL2
	vec3 dx = dFdx(worldPos);
	vec3 dy = dFdy(worldPos);
	vec3 ng = cross(dx, dy);
	float ng2 = dot(ng, ng);
	if(ng2 > 1.0e-14){
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
	vec3 L = sunLen2 > 1.0e-8 ? u_sunDir.xyz * inversesqrt(sunLen2) : vec3(0.0, 0.0, 1.0);
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

vec3 FilmicACES(vec3 hdr)
{
	vec3 x = max(hdr, vec3(0.0)) * 1.12;
	return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

vec3 ApplyFilmicToneAndGamma(vec3 hdrLinear)
{
	float luma = dot(hdrLinear, vec3(0.2126, 0.7152, 0.0722));
	vec3 vibrant = max(mix(vec3(luma), hdrLinear, 1.08), vec3(0.0));
	vec3 mapped = FilmicACES(vibrant);
	return pow(max(mapped, vec3(0.0)), vec3((1.0 / 2.2) * u_localInvGamma.x));
}

// Legacy helper kept forGL3Atmosphere=0 fallback compatibility.
vec3 SunMaterialLighting(vec3 worldPos, vec3 worldNormal, out float selfShade)
{
	float normalLength2 = dot(worldNormal, worldNormal);
	if(normalLength2 < 1.0e-8){
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
	vec3 V = toEye * inversesqrt(max(dot(toEye, toEye), 1.0e-8));
	vec3 halfDir = L + V;
	vec3 H = halfDir * inversesqrt(max(dot(halfDir, halfDir), 1.0e-8));
	float specular = pow(max(dot(N, H), 0.0), 50.0) * facing;
	return 0.2 * u_sunMaterial.rgb * day * (diffuse + mix(0.4, 0.12, cloud)*specular);
}

// Unified physically-inspired sun + sky-hemisphere + shadow-map world shading.
vec3 ShadeWorldSurface(vec3 baseSrgb, vec3 worldPos, vec3 worldNormal, float visibility)
{
	if(u_atmosphere.x < 0.5){
		float selfShade;
		vec3 legacy = baseSrgb + SunMaterialLighting(worldPos, worldNormal, selfShade);
		legacy *= 1.0 - selfShade;
		legacy = mix(u_fogColor.rgb, legacy, visibility);
		return pow(max(legacy, vec3(0.0)), vec3(u_localInvGamma.x));
	}

	float hasVertNormal;
	vec3 N = ResolveWorldNormal(worldPos, worldNormal, hasVertNormal);
	float sunLen2 = dot(u_sunDir.xyz, u_sunDir.xyz);
	vec3 L = sunLen2 > 1.0e-8 ? u_sunDir.xyz * inversesqrt(sunLen2) : vec3(0.0, 0.0, 1.0);

	float day = smoothstep(-0.04, 0.16, L.z);
	float cloud = clamp(u_atmosphere.y, 0.0, 1.0);
	float clearSun = day * (1.0 - 0.48 * cloud);
	float shadowVis = day > 0.001 ? SampleSunShadow(worldPos, N) : 1.0;

	float isFoliage = u_alphaRef.x > 0.01 ? 1.0 : 0.0;
	float rawNdotL = dot(N, L);
	float sunDiff = mix(max(rawNdotL, 0.0), 0.34 + 0.66 * clamp(rawNdotL * 0.75 + 0.25, 0.0, 1.0), isFoliage);

	float up = N.z * 0.5 + 0.5;
	vec3 coolSky = mix(vec3(0.80, 0.88, 1.03), max(u_skyAmbient.rgb, vec3(0.38, 0.48, 0.65)) * 1.38, 0.48);
	vec3 groundBounce = vec3(0.96, 0.92, 0.85);
	vec3 hemiTint = mix(groundBounce, coolSky, up);
	vec3 warmSun = mix(max(u_sunMaterial.rgb, vec3(0.55, 0.50, 0.42)), vec3(1.08, 0.97, 0.84), 0.52);

	vec3 baseLin = pow(max(baseSrgb, vec3(0.0)), vec3(2.2));
	float shadStrength = clamp(max(u_sunMaterial.a, 0.65), 0.0, 1.0) * day * (1.0 - 0.42 * cloud);

	vec3 litLin;
	if(hasVertNormal < 0.5){
		// Static buildings, roads and sidewalks without vertex normals:
		// bring full directional sun + cool architectural shadow contrast.
		float litFactor = shadowVis * smoothstep(0.0, 0.20, sunDiff);
		vec3 shadowTint = mix(vec3(1.0), vec3(0.52, 0.58, 0.69), shadStrength * (1.0 - litFactor));
		litLin = baseLin * (hemiTint * shadowTint * mix(0.90, 1.06, up) +
		                    warmSun * (0.68 * clearSun * sunDiff * shadowVis));
	}else{
		// Skinned peds & normal-bearing props already have N*L in v_color:
		// lift dark away-facing ambient with sky fill and apply cast-shadow occlusion.
		vec3 castOcclusion = mix(vec3(0.56, 0.62, 0.72), vec3(1.0), mix(1.0, shadowVis, shadStrength));
		float skyFillLift = mix(1.16, 1.02, clamp(rawNdotL, 0.0, 1.0));
		litLin = baseLin * hemiTint * castOcclusion * skyFillLift +
		         baseLin * warmSun * (0.34 * clearSun * sunDiff * shadowVis);
	}

	vec3 toEye = u_eye.xyz - worldPos;
	float dist2 = max(dot(toEye, toEye), 1.0e-6);
	float invDist = inversesqrt(dist2);
	vec3 V = toEye * invDist;
	vec3 halfVec = L + V;
	vec3 H = halfVec * inversesqrt(max(dot(halfVec, halfVec), 1.0e-8));
	float ndoth = max(dot(N, H), 0.0);

	// Sunlit road / ground micro-roughness specular sheen (like modern asphalt).
	if(N.z > 0.80 && isFoliage < 0.5 && shadowVis > 0.01 && clearSun > 0.01){
		float dist = dist2 * invDist;
		vec2 gcell = floor(worldPos.xy * 24.0);
		float grain = fract(sin(dot(gcell, vec2(12.9898, 78.233))) * 43758.5453);
		float grainBoost = mix(1.0, 0.58 + 1.10 * grain * grain, exp(-dist * 0.08));
		float roadSpec = (0.042 * pow(ndoth, 14.0) + 0.052 * pow(ndoth, 64.0)) *
		                 grainBoost * smoothstep(0.80, 0.96, N.z);
		litLin += warmSun * (roadSpec * clearSun * shadowVis * sunDiff);
	}else if(hasVertNormal > 0.5 && isFoliage < 0.5){
		float objSpec = 0.06 * pow(ndoth, 36.0) * clearSun * shadowVis * max(rawNdotL, 0.0);
		litLin += warmSun * objSpec;
	}

	vec3 toned = ApplyFilmicToneAndGamma(litLin);
	return mix(u_fogColor.rgb, toned, visibility);
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
