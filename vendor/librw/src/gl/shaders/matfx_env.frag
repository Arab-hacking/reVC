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

	// Shared geometry normal/view used by both reflection branches. The
	// paraboloid capture writes v_worldNormal under the same bit pair the
	// client uses for its reflection block (mask 0x100040, ch. 13.8.3).
	float hasVN_global;
	vec3 N0_global = ResolveWorldNormal(v_worldPos, v_worldNormal, hasVN_global);
	vec3 V0_global = normalize(u_eye.xyz - v_worldPos);

	if(u_atmosphere.x < 0.5){
		vec4 pass1 = v_color * texCol;
		vec3 refl0 = reflect(-V0_global, N0_global);
		// u_envMapMode.x: 0 = ordinary 2D env map (game TXD, u_texMatrix UV),
		// 1 = paraboloid capture. The capture pass writes NDC straight into the
		// map (0x42b527), so the lookup is the same polar projection:
		//   ReflPos = normalize(refl.xy) * (refl.z * 0.5 + 0.5)   (0x434bb8)
		//   uv      = ReflPos * 0.5 + 0.5      (NDC -> texture space)
		vec2 envUV0;
		if(u_envMapMode.x > 0.5){
			vec2 planar = refl0.xy;
			float plen2 = dot(planar, planar);
			planar = plen2 > 1.0e-12 ? planar * inversesqrt(plen2) : vec2(0.0);
			envUV0 = planar * (refl0.z * 0.5 + 0.5) * 0.5 + 0.5;
		}else{
			envUV0 = (u_texMatrix * vec4(refl0, 1.0)).xy;
		}
		// Black Russia reflection chain (13.1, verbatim):
		//   envmap.rgb = texture(tex2, ReflPos).rgb;               (0x3e1241)
		//   envmap.rgb = envmap.rgb * fresnelFactor * u_envMapCoef;(0x40b632)
		//   envmap.rgb = pow(envmap.rgb, vec3(0.8));               (0x43f1e5)
		//   color.rgb += (diffuseVec + specular + envmap.rgb);     (0x4309d9)
		// fresnelFactor = 1 - |dot(-viewVec, v_worldNormal)|       (0x43f19d)
		// No pow(,3.5) and no 0.15..0.70 clamp: those are not in the client.
		float fres0 = 1.0 - abs(dot(-V0_global, N0_global));
		vec3 envmap0 = pow(max(texture(tex1, envUV0).rgb * fres0 * shininess, vec3(0.0)), vec3(0.8));
		vec3 pass2 = envmap0 * v_envColor.rgb * visibility;
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
