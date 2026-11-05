const char *neoWorldVC_frag_src =
"uniform sampler2D tex0;\n"
"uniform sampler2D tex1;\n"

"uniform vec4 u_lightMap;\n"
"uniform float u_brLightmap; // only previously marked UV2 textures use this alternate alpha mix\n"

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

"	vec4 color;\n"
"	vec3 nativeLight = t0.rgb*v_color.rgb*(1.0 + u_lightMap.rgb*(t1.rgb-1.0));\n"
"	float brWeight = clamp(t1.a*u_lightMap.x, 0.0, 1.0) * u_brLightmap;\n"
"	vec3 baseLit = mix(nativeLight, t0.rgb*v_color.rgb, u_brLightmap);\n"
"	color.a = v_color.a*t0.a*u_lightMap.a;\n"
"	DoAlphaTest(color.a);\n"

"	float visibility = AtmosphereVisibility(v_worldPos, v_fog);\n"
"	vec3 worldShaded = ShadeWorldSurface(baseLit, v_worldPos, v_worldNormal, visibility);\n"
"	vec3 bakedGlow = mix(u_fogColor.rgb, clamp(t0.rgb*t1.rgb*2.6, 0.0, 1.5), visibility);\n"
"	color.rgb = clamp(mix(worldShaded, max(worldShaded, bakedGlow), brWeight), 0.0, 1.0);\n"

"	FRAGCOLOR(color);\n"
"}\n"
;
