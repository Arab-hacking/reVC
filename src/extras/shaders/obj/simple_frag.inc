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
"	vec4 texCol = texture(tex0, vec2(v_tex0.x, 1.0-v_tex0.y));\n"
"	vec4 color;\n"
"	color.a = texCol.a * v_color.a;\n"
"	DoAlphaTest(color.a);\n"

"	float visibility = AtmosphereVisibility(v_worldPos, v_fog);\n"
"	color.rgb = ShadeWorldSurfaceEx(texCol.rgb, v_color.rgb, v_worldPos, v_worldNormal, visibility, 1.0);\n"

"	FRAGCOLOR(color);\n"
"}\n"
;
