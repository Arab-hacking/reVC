uniform sampler2D tex0;
uniform sampler2D tex1;
uniform vec4 u_ambLight;
uniform vec4 u_reflProps;
uniform vec4 u_specDir[5];
uniform vec4 u_specColor[5];

#define fresnel (u_reflProps.x)
#define lightStrength (u_reflProps.y)
#define shininess (u_reflProps.z)
#define specularity (u_reflProps.w)

FSIN vec4 v_color;
FSIN vec4 v_prelit;
FSIN vec4 v_lightFog;
FSIN vec3 v_pointSpot;
FSIN vec2 v_tex0;
FSIN vec3 v_worldPos;
FSIN vec3 v_worldNormal;

float
DoDirLightSpec(vec3 N, vec3 V, vec3 L, float power)
{
	vec3 H = normalize(V + L);
	return pow(clamp(dot(N, H), 0.0, 1.0), power);
}

// ParaboloidEnvMap.glsl adapted to EnvMapCam (+Z zenith at center (0.5, 0.5)).
// EnvMapTex is an LDR tonemapped raster, so undo ACES 1.8x exposure_bias (0.68)
// before re-tonemapping in BR_ShadeMaterialHDR.
vec3
SampleVehicleParaboloidEnv(vec3 roughR, float linearRoughness, vec3 L, vec3 sunDirectColor, vec3 sunAmbientColor)
{
	vec3 r = roughR * inversesqrt(max(dot(roughR, roughR), 1.0e-6));
	vec3 rp = normalize(vec3(r.xy, abs(r.z) + 1.0e-5));
	vec2 envUV = (rp.xy / (1.0 + rp.z)) * 0.485 + 0.5;
	vec3 texEnvLin = BR_SRGBtoLINEAR(texture(tex1, vec2(envUV.x, 1.0 - envUV.y))).rgb * 0.68;

	float hemiMask = smoothstep(-0.28, 0.14, r.z);
	vec3 analyticSky = BR_EvalAnalyticalSkyEnv(r, L, sunDirectColor, sunAmbientColor);
	vec3 crispEnv = mix(analyticSky * 0.75, texEnvLin, hemiMask * 0.88);
	vec3 roughSky = BR_GetSkySHDiffuse(r, sunAmbientColor);
	return mix(crispEnv, roughSky, smoothstep(0.15, 0.85, linearRoughness));
}

void
main(void)
{
	vec4 texSample = texture(tex0, vec2(v_tex0.x, 1.0-v_tex0.y));
	float alpha = clamp(v_color.a * texSample.a, 0.0, 1.0);
	DoAlphaTest(alpha);

	float hasVertNormal;
	vec3 N = ResolveWorldNormal(v_worldPos, v_worldNormal, hasVertNormal);
	vec3 toEye = u_eye.xyz - v_worldPos;
	vec3 V = toEye * inversesqrt(max(dot(toEye, toEye), 1.0e-6));
	float visibility = AtmosphereVisibility(v_worldPos, v_lightFog.w);

	// Legacy fallback when GL3Atmosphere=0
	if(u_atmosphere.x < 0.5){
		vec3 legacyVert = clamp(v_prelit.rgb + u_ambLight.rgb * u_surfProps.x + v_lightFog.rgb, 0.0, 1.4);
		vec3 pass1 = legacyVert * v_color.rgb * texSample.rgb;
		float ndotv0 = clamp(abs(dot(V, N)), 0.0, 1.0);
		vec3 R0 = reflect(-V, N);
		vec3 Rp0 = normalize(vec3(R0.xy, abs(R0.z) + 1.0e-5));
		vec2 envUV0 = (Rp0.xy / (1.0 + Rp0.z)) * 0.485 + 0.5;
		vec3 env0 = texture(tex1, vec2(envUV0.x, 1.0 - envUV0.y)).rgb;
		float reflFactor0 = mix(pow(1.0 - ndotv0, 4.0), 1.0, fresnel) * shininess;
		vec3 pass2 = env0 * reflFactor0 * lightStrength;
		for(int i = 0; i < 5; i++)
			pass2 += DoDirLightSpec(N, V, -u_specDir[i].xyz, u_specDir[i].w) * u_specColor[i].rgb * specularity;
		vec4 legacyOut;
		legacyOut.rgb = mix(u_fogColor.rgb, pass1, visibility) * alpha + pass2 * visibility;
		legacyOut.a = alpha;
		legacyOut.rgb = pow(max(legacyOut.rgb, vec3(0.0)), vec3(u_localInvGamma.x));
		FRAGCOLOR(legacyOut);
		return;
	}

	// --- Exact Vehicle.fs (Shaders For BLAYZ RUSSIA PC) ---
	float isUnlit = step(1.5, u_surfProps.w);
	float hasTex = step(0.5, u_surfProps.w - 2.0 * isUnlit);
	float isLitLamp = max(isUnlit, step(1.25, u_surfProps.x));
	float texLum = br_luminance(texSample.rgb);
	float isOpaque = step(0.95, alpha);

	float rawEnv = clamp(u_surfProps.y / 0.72, 0.0, 1.0);
	float rubberKill = mix(1.0, smoothstep(0.04, 0.18, texLum), hasTex * isOpaque);
	float envMapCoef = rawEnv * rubberKill;

	float isChrome = smoothstep(0.86, 0.96, u_surfProps.y) * mix(1.0, smoothstep(0.18, 0.52, texLum), hasTex);
	float isGlass = 1.0 - smoothstep(0.72, 0.96, alpha);

	// Vehicle.fs lines 57-60:
	// smoothness = mix(0.3, 0.95, u_envMapCoef);
	// metallness = mix(0.0, 0.1, u_envMapCoef);
	// reflectance = mix(DEFAULT_REFLECTANCE, 0.8, u_envMapCoef);
	float smoothness = mix(0.30, 0.95, max(envMapCoef, max(isChrome, isGlass)));
	float metalness = mix(mix(0.0, 0.10, envMapCoef), 0.88, isChrome) * isOpaque;
	float reflectance = mix(BR_DEFAULT_REFLECTANCE, 0.80, max(envMapCoef, isGlass));

	// Vehicle.fs lines 43-49: SRGBtoLINEAR(tex0) * matColor
	vec3 baseSrgb = v_color.rgb * texSample.rgb;
	vec3 albedoLin = BR_SRGBtoLINEAR(vec4(baseSrgb, 1.0)).rgb * (vec3(1.0) + v_pointSpot);

	float sunLen2 = dot(u_sunDir.xyz, u_sunDir.xyz);
	vec3 L = sunLen2 > 1.0e-6 ? u_sunDir.xyz * inversesqrt(sunLen2) : vec3(0.0, 0.0, 1.0);
	vec3 sunDirectColor;
	vec3 sunAmbientColor;
	float dayFactor;
	BR_GetSceneLightColors(L, sunDirectColor, sunAmbientColor, dayFactor);

	float linRough = clamp(1.0 - smoothness, 0.04, 1.0);
	float ggxAlpha = max(1.0e-4, linRough * linRough);
	vec3 roughR = BR_GetRoughReflectionVec(reflect(-V, N), N, ggxAlpha);
	vec3 customEnvLin = SampleVehicleParaboloidEnv(roughR, linRough, L, sunDirectColor, sunAmbientColor);

	float whitePoint = max(u_skyAmbient.a, 2.5);
	vec4 color;

	if(isOpaque > 0.5){
		vec3 hdr = BR_ShadeMaterialHDR(
			albedoLin, v_worldPos, N,
			smoothness, metalness, reflectance, 1.0,
			0.0, 0.0, 1.65 * isLitLamp, 0.82 * isLitLamp,
			customEnvLin, 1.0);
		color.rgb = mix(u_fogColor.rgb, BR_TonemapAces(hdr, whitePoint, 2.2), visibility);
		color.a = 1.0;
	}else{
		// Premultiplied-alpha composite for vehicle windshields (SRCBLEND=ONE, DESTBLEND=INVSRCALPHA)
		vec3 baseHdr = BR_ShadeMaterialHDR(
			albedoLin, v_worldPos, N,
			0.22, 0.0, 0.25, 1.0,
			0.0, 0.0, 1.65 * isLitLamp, 0.82 * isLitLamp,
			vec3(0.0), 0.0);
		vec3 fullHdr = BR_ShadeMaterialHDR(
			albedoLin, v_worldPos, N,
			smoothness, 0.0, reflectance, 1.0,
			0.0, 0.0, 1.65 * isLitLamp, 0.82 * isLitLamp,
			customEnvLin, 1.0);
		vec3 specHdr = max(fullHdr - baseHdr, vec3(0.0));
		vec3 baseLdr = BR_TonemapAces(baseHdr, whitePoint, 2.2);
		vec3 specLdr = BR_TonemapAces(specHdr, whitePoint, 2.2);
		float ndotv = clamp(abs(dot(V, N)), 0.0, 1.0);
		float glassFres = 0.08 + 0.62 * br_pow5(1.0 - ndotv);
		color.rgb = clamp(mix(u_fogColor.rgb, baseLdr, visibility) * alpha + specLdr * visibility, 0.0, 1.0);
		color.a = clamp(max(alpha, glassFres * 0.55), 0.0, 1.0);
	}

	FRAGCOLOR(color);
}
