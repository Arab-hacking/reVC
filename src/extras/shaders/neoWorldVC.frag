uniform sampler2D tex0;
uniform sampler2D tex1;
uniform vec4 u_lightMap;
uniform float u_brLightmap;

#define u_lm (vec4(u_lightMap.rgb, 0.0))

FSIN vec4 v_color;
FSIN vec2 v_tex0;
FSIN vec2 v_tex1;
FSIN float v_fog;
FSIN vec3 v_worldPos;
FSIN vec3 v_worldNormal;

void
main(void)
{
	vec4 t0 = texture(tex0, vec2(v_tex0.x, 1.0-v_tex0.y));
	vec4 t1 = texture(tex1, vec2(v_tex1.x, 1.0-v_tex1.y));

	float visibility = AtmosphereVisibility(v_worldPos, v_fog);

	if(u_brLightmap > 0.5 && u_atmosphere.x > 0.5){
		float outAlpha = t0.a * v_color.a;
		DoAlphaTest(outAlpha);

		if(u_atmosphere.x >= 1.5){
			float mDayNight = clamp(u_atmosphere.w, 0.0, 1.0);
			float mLmMulti = (u_skyAmbient.a > 0.01 && u_skyAmbient.a < 2.0) ? u_skyAmbient.a : 0.65;
			float mLmFade = clamp(mDayNight * t1.a * step(0.04, dot(t1.rgb, vec3(0.3333))), 0.0, 1.0);
			vec3 mLit = BR_ShadeMobileWorldLinear(t0.rgb, v_color.rgb, v_worldPos, v_worldNormal, 0.0);
			vec3 mGlow = t1.rgb * max(t0.rgb, vec3(0.22)) * (3.0 * mLmMulti);
			vec3 mCombined = mix(mLit, max(mLit, mGlow), mLmFade);
			vec4 mColor;
			mColor.rgb = BR_ApplyMobileColorGrade(mix(u_fogColor.rgb, mCombined, visibility));
			mColor.a = outAlpha;
			FRAGCOLOR(mColor);
			return;
		}

		// Exact Building.fs + LightMapEmissive.glsl (Shaders For BLAYZ RUSSIA PC):
		// BR dual-UV night lightmap emits with emissionStrength = 2.0 at night.

		float dayNight = clamp(u_atmosphere.w, 0.0, 1.0);
		vec4 lmLin = BR_SRGBtoLINEAR(t1);
		float lightMapFade = dayNight * step(0.1, length(lmLin.rgb)) * lmLin.a;

		vec3 baseLin = BR_SRGBtoLINEAR(t0).rgb * max(v_color.rgb, vec3(0.0));
		vec3 albedoLin = mix(baseLin, lmLin.rgb, lightMapFade);

		float hasVertNormal;
		vec3 N = ResolveWorldNormal(v_worldPos, v_worldNormal, hasVertNormal);
		float isFoliage = u_alphaRef.x > 0.01 ? 1.0 : 0.0;
		if(isFoliage > 0.5 && hasVertNormal < 0.5)
			N = normalize(mix(N, vec3(0.0, 0.0, 1.0), 0.70));

		vec3 hdr = BR_ShadeMaterialHDR(
			albedoLin, v_worldPos, N,
			0.20, 0.0, BR_DEFAULT_REFLECTANCE, 1.0,
			isFoliage, isFoliage * 0.42,
			2.0 * lightMapFade, lightMapFade,
			vec3(0.0), 0.0);

		float whitePoint = max(u_skyAmbient.a, 2.5);
		vec4 color;
		color.rgb = mix(u_fogColor.rgb, BR_TonemapAces(hdr, whitePoint, 2.2), visibility);
		color.a = outAlpha;
		FRAGCOLOR(color);
		return;
	}

	vec4 surface = mix(t0, t0*t1*(1.0 + u_lm), t0.a*u_lightMap.a);
	surface.a = t0.a*v_color.a;
	DoAlphaTest(surface.a);

	vec4 color;
	color.rgb = ShadeWorldSurfaceEx(surface.rgb, v_color.rgb, v_worldPos, v_worldNormal, visibility, 0.0);
	color.a = surface.a;

	FRAGCOLOR(color);
}
