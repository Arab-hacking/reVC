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

"	vec4 surface = mix(t0, t0*t1*(1.0 + u_lm), t0.a*u_lightMap.a);\n"
"	surface.a = t0.a*v_color.a;\n"
"	DoAlphaTest(surface.a);\n"

"	float visibility = AtmosphereVisibility(v_worldPos, v_fog);\n"
"	vec4 color;\n"
"	color.rgb = ShadeWorldSurfaceEx(surface.rgb, v_color.rgb, v_worldPos, v_worldNormal, visibility, 0.0);\n"
"	color.a = surface.a;\n"

"	FRAGCOLOR(color);\n"
"}\n"
;
