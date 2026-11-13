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
	// Must invert exactly what the capture pass wrote (header.vert,
	// ParaboloidProjection / client 0x42b527):
	//   NDC.xy = normalize(r.xy) * (r.z * 0.5 + 0.5)
	// so the lookup is the same polar mapping, not a stereographic one.
	vec3 r = roughR * inversesqrt(max(dot(roughR, roughR), 1.0e-6));
	vec2 rplanar = r.xy;
	float rlen2 = dot(rplanar, rplanar);
	rplanar = rlen2 > 1.0e-12 ? rplanar * inversesqrt(rlen2) : vec2(0.0);
	vec2 envUV = rplanar * (r.z * 0.5 + 0.5) * 0.5 + 0.5;
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

	if(u_atmosphere.x >= 1.5){
		vec3 mBase = v_color.rgb * texSample.rgb;
		if(isLitLamp > 0.5){
			vec4 lampOut;
			lampOut.rgb = BR_ApplyMobileColorGrade(mix(u_fogColor.rgb, clamp(mBase * 1.35, 0.0, 1.0), visibility));
			lampOut.a = alpha;
			FRAGCOLOR(lampOut);
			return;
		}
		float mSunLen2 = dot(u_sunDir.xyz, u_sunDir.xyz);
		vec3 mL = mSunLen2 > 1.0e-6 ? u_sunDir.xyz * inversesqrt(mSunLen2) : vec3(0.0, 0.0, 1.0);
		float mDay = smoothstep(-0.04, 0.20, mL.z);
		float mHighSun = smoothstep(0.02, 0.55, mL.z);
		float mCloud = clamp(u_atmosphere.y, 0.0, 1.0);
		float mShadow = mDay > 0.001 ? SampleSunShadow(v_worldPos, N) : 1.0;

		float mHemi = clamp(N.z * 0.5 + 0.5, 0.0, 1.0);
		// Same rebalance as BR_ShadeMobileWorldLinear: the cold sky term used to
		// make up ~82% of the light and painted every body blue-grey. Ambient is
		// a fill now and the sun carries the contrast, otherwise cars and the
		// world they stand on drift apart.
		vec3 mDayAmb = mix(vec3(0.84, 0.85, 0.88), vec3(0.92, 0.94, 0.98), mHighSun);
		vec3 mSkyTint = mix(mDayAmb, clamp(u_skyAmbient.rgb, vec3(0.30), vec3(1.10)), 0.12);
		vec3 mAmb = mix(vec3(0.34, 0.39, 0.52), mSkyTint, mDay) *
		           mix(mix(0.26, 0.34, mHemi), mix(0.10, 0.15, mHemi) * (1.0 - 0.18 * mCloud), mDay);

		float mNdotL = max(dot(N, mL), 0.0);
		vec3 mSunLightCol = mix(vec3(1.06, 0.94, 0.76), vec3(0.98, 0.97, 0.95), mHighSun);
		mSunLightCol = mix(mSunLightCol, mSunLightCol * clamp(u_sunMaterial.rgb, vec3(0.60), vec3(1.15)), 0.25);
		vec3 mDiffVec = mNdotL * mBase * mSunLightCol * (mDay * (1.0 - 0.35 * mCloud) * 0.80);

		vec3 mH = mL + V;
		mH *= inversesqrt(max(dot(mH, mH), 1.0e-6));
		float isGlassM = 1.0 - smoothstep(0.72, 0.96, alpha);
		float mEffEnv = max(envMapCoef, isGlassM * 0.85);
		float mSpecAmt = pow(max(dot(N, mH), 0.0), 50.0) * 0.40 * mDay * (0.35 + 0.65 * mEffEnv) * (1.0 - 0.45 * mCloud);
		vec3 mSpecVec = mSunLightCol * (mSpecAmt * 2.5);

		// Mobile ParaboloidEnvMap + Fresnel [0.15 .. 0.70], pow(env, 0.8), additive composition
		float mNoV = clamp(abs(dot(V, N)), 0.0, 1.0);
		float mFresnel = clamp(pow(1.0 - mNoV, 3.5), 0.15, 0.70);
		vec3 mR = reflect(-V, N);
		mR *= inversesqrt(max(dot(mR, mR), 1.0e-6));
		float mRxy2 = dot(mR.xy, mR.xy);
		vec2 mDirXY = mRxy2 > 1.0e-6 ? (mR.xy * inversesqrt(mRxy2)) : vec2(0.0, 0.0);
		vec2 mEnvUV = mDirXY * ((1.0 - abs(mR.z)) * 0.485) + vec2(0.5);
		vec3 mTexRefl = pow(max(texture(tex1, vec2(mEnvUV.x, 1.0 - mEnvUV.y)).rgb, vec3(0.0)), vec3(0.8));
		mTexRefl *= mix(0.48, 1.0, smoothstep(-0.42, 0.06, mR.z));
		vec3 mEnvAdd = mTexRefl * (mFresnel * mEffEnv);

		vec3 mDynDiff = v_pointSpot * 0.55;
		vec3 mDynSpec = vec3(0.0);
		for(int li = 0; li < 2; li++){
			float rad = u_sceneLightPosRad[li].w;
			if(rad <= 0.01)
				break;
			vec3 toLight = u_sceneLightPosRad[li].xyz - v_worldPos;
			float d2 = dot(toLight, toLight);
			float r2 = rad * rad;
			if(d2 >= r2)
				continue;
			vec3 Lp = toLight * inversesqrt(max(d2, 1.0e-4));
			float x2 = d2 / r2;
			float atten = br_pow2(max(1.0 - x2, 0.0)) / (1.0 + 2.6 * x2);
			if(u_sceneLightDir[li].w > 0.5)
				atten *= smoothstep(0.30, 0.78, dot(-Lp, u_sceneLightDir[li].xyz));
			float wrapDiff = max(dot(N, Lp) * 0.78 + 0.22, 0.0);
			vec3 Hp = normalize(Lp + V);
			float lSpec = pow(max(dot(N, Hp), 0.0), 50.0) * 0.40 * (0.25 + 0.75 * mEffEnv);
			vec3 lCol = u_sceneLightColor[li].rgb * atten;
			mDynDiff += lCol * (wrapDiff * 0.85);
			mDynSpec += lCol * lSpec;
		}

		vec3 mDiffuseLit = (mBase * mAmb + mDiffVec) * mShadow + mBase * mDynDiff;
		vec3 mGlossAdd = mSpecVec * mShadow + mDynSpec + mEnvAdd;
		vec3 mCar = mDiffuseLit + mGlossAdd;
		if(isGlassM > 0.05){
			vec4 glassOut;
			vec3 gBase = BR_ApplyMobileColorGrade(mix(u_fogColor.rgb, mDiffuseLit, visibility));
			vec3 gSpec = BR_ApplyMobileColorGrade(mGlossAdd * visibility);
			glassOut.rgb = clamp(gBase * alpha + gSpec, 0.0, 1.0);
			glassOut.a = clamp(max(alpha, mFresnel * 0.48), 0.0, 1.0);
			FRAGCOLOR(glassOut);
			return;
		}
		vec4 mobOut;
		mobOut.rgb = BR_ApplyMobileColorGrade(mix(u_fogColor.rgb, mCar, visibility));
		mobOut.a = alpha;
		FRAGCOLOR(mobOut);
		return;
	}

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
