uniform sampler2D tex0;
uniform sampler2D tex1;
uniform mat4 u_texMatrix;

uniform vec4 u_fxparams;

#define shininess (u_fxparams.x)
#define disableFBA (u_fxparams.y)

FSIN vec4 v_color;
FSIN vec4 v_envColor;
FSIN vec2 v_tex0;
FSIN float v_fog;
FSIN vec3 v_worldPos;
FSIN vec3 v_worldNormal;

void
main(void)
{
	vec4 texCol = texture(tex0, vec2(v_tex0.x, 1.0-v_tex0.y));
	float outAlpha = clamp(texCol.a * v_color.a, 0.0, 1.0);
	DoAlphaTest(outAlpha);

	float visibility = AtmosphereVisibility(v_worldPos, v_fog);
	float fba = max(outAlpha, disableFBA);

	if(u_atmosphere.x < 0.5){
		vec4 pass1 = v_color * texCol;
		float hasVN0;
		vec3 N0 = ResolveWorldNormal(v_worldPos, v_worldNormal, hasVN0);
		vec3 V0 = normalize(u_eye.xyz - v_worldPos);
		vec3 refl0 = reflect(-V0, N0);
		vec2 envUV0 = (u_texMatrix * vec4(refl0, 1.0)).xy;
		vec3 envmap0 = pow(max(texture(tex1, envUV0).rgb, vec3(0.0)), vec3(0.8));
		float fres0 = clamp(pow(1.0 - clamp(abs(dot(V0, N0)), 0.0, 1.0), 3.5), 0.15, 0.70);
		vec3 pass2 = envmap0 * v_envColor.rgb * fres0 * shininess * visibility;
		pass1.rgb = ShadeWorldSurfaceEx(texCol.rgb, v_color.rgb, v_worldPos, v_worldNormal, visibility, 0.0);
		vec4 legacyCol;
		legacyCol.rgb = clamp(pass1.rgb * outAlpha + pass2 * fba, 0.0, 1.0);
		legacyCol.a = outAlpha;
		FRAGCOLOR(legacyCol);
		return;
	}

	float hasVertNormal;
	vec3 N = ResolveWorldNormal(v_worldPos, v_worldNormal, hasVertNormal);
	vec3 toEye = u_eye.xyz - v_worldPos;
	vec3 V = toEye * inversesqrt(max(dot(toEye, toEye), 1.0e-6));
	float envCoef = clamp(shininess, 0.0, 1.0);

	if(u_atmosphere.x >= 1.5){
		vec3 mRefl = reflect(-V, N);
		vec2 mEnvUV = (u_texMatrix * vec4(mRefl, 1.0)).xy;
		vec3 mEnvTex = pow(max(texture(tex1, mEnvUV).rgb, vec3(0.0)), vec3(0.8)) * v_envColor.rgb;
		float mNoV = clamp(abs(dot(V, N)), 0.0, 1.0);
		float mFres = clamp(pow(1.0 - mNoV, 3.5), 0.15, 0.70);
		float mSunLen2 = dot(u_sunDir.xyz, u_sunDir.xyz);
		vec3 mL = mSunLen2 > 1.0e-6 ? u_sunDir.xyz * inversesqrt(mSunLen2) : vec3(0.0, 0.0, 1.0);
		float mDay = smoothstep(-0.04, 0.20, mL.z);
		vec3 mH = normalize(mL + V);
		float mSunSpec = pow(max(dot(N, mH), 0.0), 50.0) * 0.40 * mDay * envCoef;
		vec3 mSpec = (mEnvTex * (mFres * envCoef) + vec3(mSunSpec * 0.5)) * visibility;
		vec3 mBase = BR_ShadeMobileWorldLinear(texCol.rgb, v_color.rgb, v_worldPos, v_worldNormal, 0.0);
		vec4 mOut;
		if(outAlpha >= 0.98){
			mOut.rgb = BR_ApplyMobileColorGrade(mix(u_fogColor.rgb, mBase, visibility) + mSpec);
			mOut.a = outAlpha;
		}else{
			vec3 mGradedBase = BR_ApplyMobileColorGrade(mix(u_fogColor.rgb, mBase, visibility));
			vec3 mGradedSpec = BR_ApplyMobileColorGrade(mSpec);
			mOut.rgb = clamp(mGradedBase * outAlpha + mGradedSpec * fba, 0.0, 1.0);
			mOut.a = clamp(outAlpha + br_luminance(mGradedSpec) * 0.35 * (1.0 - outAlpha), 0.0, 1.0);
		}
		FRAGCOLOR(mOut);
		return;
	}

	float smoothness = mix(0.25, 0.92, envCoef);
	float linRough = 1.0 - smoothness;
	float ggxAlpha = max(1.0e-4, linRough * linRough);

	vec3 roughR = BR_GetRoughReflectionVec(reflect(-V, N), N, ggxAlpha);
	vec2 envUV = (u_texMatrix * vec4(roughR, 1.0)).xy;
	vec3 envLin = BR_SRGBtoLINEAR(texture(tex1, envUV)).rgb * v_envColor.rgb;
	vec3 albedoLin = BR_SRGBtoLINEAR(texCol).rgb * max(v_color.rgb, vec3(0.0));
	float whitePoint = max(u_skyAmbient.a, 2.5);

	vec4 color;
	if(outAlpha >= 0.98){
		vec3 hdr = BR_ShadeMaterialHDR(
			albedoLin, v_worldPos, N,
			smoothness, mix(0.0, 0.08, envCoef), mix(BR_DEFAULT_REFLECTANCE, 0.78, envCoef), 1.0,
			0.0, 0.0, 0.0, 0.0,
			envLin, step(0.02, envCoef));
		color.rgb = mix(u_fogColor.rgb, BR_TonemapAces(hdr, whitePoint, 2.2), visibility);
		color.a = outAlpha;
	}else{
		vec3 baseHdr = BR_ShadeMaterialHDR(
			albedoLin, v_worldPos, N,
			0.20, 0.0, 0.25, 1.0,
			0.0, 0.0, 0.0, 0.0,
			vec3(0.0), 0.0);
		vec3 fullHdr = BR_ShadeMaterialHDR(
			albedoLin, v_worldPos, N,
			max(smoothness, 0.90), 0.0, 0.80, 1.0,
			0.0, 0.0, 0.0, 0.0,
			envLin, step(0.02, envCoef));
		vec3 specHdr = max(fullHdr - baseHdr, vec3(0.0));
		vec3 baseLdr = BR_TonemapAces(baseHdr, whitePoint, 2.2);
		vec3 specLdr = BR_TonemapAces(specHdr, whitePoint, 2.2);
		// gl3matfx uses SRCBLEND=ONE, DESTBLEND=INVSRCALPHA
		color.rgb = clamp(mix(u_fogColor.rgb, baseLdr, visibility) * outAlpha + specLdr * fba * visibility, 0.0, 1.0);
		color.a = clamp(outAlpha + br_luminance(specLdr) * 0.35 * (1.0 - outAlpha), 0.0, 1.0);
	}

	FRAGCOLOR(color);
}
