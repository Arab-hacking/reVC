const char *neoWorldVC_frag_src =
"uniform sampler2D tex0;\n"
"uniform sampler2D tex1;\n"

"uniform vec4 u_lightMap;\n"
"uniform float u_brLightmap; // only marked zip/ UV2 lightmaps use BR's alpha mix\n"

"FSIN vec4 v_color;\n"
"FSIN vec2 v_tex0;\n"
"FSIN vec2 v_tex1;\n"
"FSIN float v_fog;\n"

"void\n"
"main(void)\n"
"{\n"
"	vec4 t0 = texture(tex0, vec2(v_tex0.x, 1.0-v_tex0.y));\n"
"	vec4 t1 = texture(tex1, vec2(v_tex1.x, 1.0-v_tex1.y));\n"

"	vec4 color;\n"
"	vec3 nativeLight = t0.rgb*v_color.rgb*(1.0 + u_lightMap.rgb*(t1.rgb-1.0));\n"
"	// BR tex1 is a lightmap in UV2, mixed by its ALPHA; the lit texture\n"
"	// contributes up to three times its albedo. Do not apply this to native\n"
"	// MatFX DUAL detail textures or unmarked model materials.\n"
"	vec3 brLight = mix(t0.rgb*v_color.rgb, t0.rgb*t1.rgb*3.0,\n"
"	                   clamp(t1.a*u_lightMap.x, 0.0, 1.0));\n"
"	color.rgb = mix(nativeLight, brLight, u_brLightmap);\n"
"	color.a = v_color.a*t0.a*u_lightMap.a;\n"

"	color.rgb = mix(u_fogColor.rgb, color.rgb, v_fog);\n"
"	DoAlphaTest(color.a);\n"

"	FRAGCOLOR(color);\n"
"}\n"

;
