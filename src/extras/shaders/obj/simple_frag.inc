const char *simple_frag_src =
"uniform sampler2D tex0;\n"

"FSIN vec4 v_color;\n"
"FSIN vec2 v_tex0;\n"
"FSIN float v_fog;\n"
"FSIN vec3 v_worldPos;\n"
"FSIN vec3 v_worldNormal;\n"

"void\n"
"main(void)\n"
"{\n"
"	vec4 color = v_color*texture(tex0, vec2(v_tex0.x, 1.0-v_tex0.y));\n"
"	DoAlphaTest(color.a);\n"
"	float visibility = AtmosphereVisibility(v_worldPos, v_fog);\n"
"	color.rgb = ShadeWorldSurface(color.rgb, v_worldPos, v_worldNormal, visibility);\n"

"	FRAGCOLOR(color);\n"
"}\n"
;
