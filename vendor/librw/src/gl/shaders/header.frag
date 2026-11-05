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

// --- Black Russia client material parity ---
uniform vec4 u_eye;		// xyz: camera world position
uniform vec4 u_sunDir;	// xyz: direction TO the sun
uniform vec4 u_sunMaterial;	// rgb: directional timecycle colour, a: shadow strength
uniform vec4 u_atmosphere;	// x: enabled, y: clouds, z: fogginess (0..1)
uniform vec4 u_localInvGamma;	// x: gamma exponent

// Per-fragment aerial perspective, bounded by the timecycle's fog start and
// VC's existing far clip. The normalized exponential meets the fog/sky colour
// exactly at the far plane; weather increases the mid-distance haze. No extra
// depth pass or blur is needed and disabling it restores the previous fog.
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

// Additional per-pixel sun lighting for the GL3 world, skin and MatFX shaders.
// The normal is interpolated, not the diffuse coefficient: broad triangles and
// skinned meshes get the same soft terminator. This is only self-shading; the
// game's projected object/vehicle shadows are still drawn separately.
vec3 SunMaterialLighting(vec3 worldPos, vec3 worldNormal, out float selfShade)
{
	float normalLength2 = dot(worldNormal, worldNormal);
	if(normalLength2 < 1.0e-8){
		selfShade = 0.0; // unlit geometry without normals
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
