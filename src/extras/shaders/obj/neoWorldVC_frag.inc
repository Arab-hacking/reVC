const char *neoWorldVC_frag_src =
"uniform sampler2D tex0;\n"
"uniform sampler2D tex1;\n"
"uniform vec4 u_lightMap;\n"
"uniform float u_brLightmap;\n"

"#define u_lm (vec4(u_lightMap.rgb, 0.0))\n"

"FSIN vec4 v_color;\n"
"FSIN vec2 v_tex0;\n"
"FSIN vec2 v_tex1;\n"
"FSIN float v_fog;\n"
"FSIN vec3 v_worldPos;\n"
"FSIN vec3 v_worldNormal;\n"

"void\n"
"main(void)\n"
"{\n"
"	vec4 t0 = texture(tex0, vec2(v_tex0.x, 1.0-v_tex0.y));\n"
"	vec4 t1 = texture(tex1, vec2(v_tex1.x, 1.0-v_tex1.y));\n"

"	float visibility = AtmosphereVisibility(v_worldPos, v_fog);\n"

"	if(u_brLightmap > 0.5 && u_atmosphere.x > 0.5){\n"
"		// Exact Building.fs + LightMapEmissive.glsl (Shaders For BLAYZ RUSSIA PC):\n"
"		// BR dual-UV night lightmap emits with emissionStrength = 2.0 at night.\n"
"		float outAlpha = t0.a * v_color.a;\n"
"		DoAlphaTest(outAlpha);\n"

"		float dayNight = clamp(u_atmosphere.w, 0.0, 1.0);\n"
"		vec4 lmLin = BR_SRGBtoLINEAR(t1);\n"
"		float lightMapFade = dayNight * step(0.1, length(lmLin.rgb)) * lmLin.a;\n"

"		vec3 baseLin = BR_SRGBtoLINEAR(t0).rgb * max(v_color.rgb, vec3(0.0));\n"
"		vec3 albedoLin = mix(baseLin, lmLin.rgb, lightMapFade);\n"

"		float hasVertNormal;\n"
"		vec3 N = ResolveWorldNormal(v_worldPos, v_worldNormal, hasVertNormal);\n"
"		float isFoliage = u_alphaRef.x > 0.01 ? 1.0 : 0.0;\n"
"		if(isFoliage > 0.5 && hasVertNormal < 0.5)\n"
"			N = normalize(mix(N, vec3(0.0, 0.0, 1.0), 0.70));\n"

"		vec3 hdr = BR_ShadeMaterialHDR(\n"
"			albedoLin, v_worldPos, N,\n"
"			0.20, 0.0, BR_DEFAULT_REFLECTANCE, 1.0,\n"
"			isFoliage, isFoliage * 0.42,\n"
"			2.0 * lightMapFade, lightMapFade,\n"
"			vec3(0.0), 0.0);\n"

"		float whitePoint = max(u_skyAmbient.a, 2.5);\n"
"		vec4 color;\n"
"		color.rgb = mix(u_fogColor.rgb, BR_TonemapAces(hdr, whitePoint, 2.2), visibility);\n"
"		color.a = outAlpha;\n"
"		FRAGCOLOR(color);\n"
"		return;\n"
"	}\n"

"	vec4 surface = mix(t0, t0*t1*(1.0 + u_lm), t0.a*u_lightMap.a);\n"
"	surface.a = t0.a*v_color.a;\n"
"	DoAlphaTest(surface.a);\n"

"	vec4 color;\n"
"	color.rgb = ShadeWorldSurfaceEx(surface.rgb, v_color.rgb, v_worldPos, v_worldNormal, visibility, 0.0);\n"
"	color.a = surface.a;\n"

"	FRAGCOLOR(color);\n"
"}\n"
;
