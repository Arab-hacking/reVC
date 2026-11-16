const char *contrast_frag_src =
"uniform sampler2D tex0;\n"
"uniform vec3 u_contrastAdd;\n"
"uniform vec3 u_contrastMult;\n"
"// Posteffects/Vignette.fs from the client:\n"
"//   radius = sqrt(pow2(uv.x - 0.5) * params.x + pow2(uv.y - 0.5));\n"
"//   fade   = clamp(radius * params.y + params.z, 0.0, 1.0);\n"
"//   color  = mix(color, vignetteColor.rgb, fade * vignetteColor.a);\n"
"// u_vignetteColor.a == 0 keeps this a pure contrast step, which is what the\n"
"// port did before, so the default look is unchanged until something enables it.\n"
"uniform vec4 u_vignette;\n"
"uniform vec4 u_vignetteColor;\n"

"FSIN vec4 v_color;\n"
"FSIN vec2 v_tex0;\n"
"FSIN float v_fog;\n"

"void\n"
"main(void)\n"
"{\n"
"	vec2 uv = vec2(v_tex0.x, 1.0-v_tex0.y);\n"
"	vec4 dst = texture(tex0, uv);\n"
"	vec3 color = dst.rgb*u_contrastMult + u_contrastAdd;\n"
"	if(u_vignetteColor.a > 0.0){\n"
"		float radius = sqrt((uv.x - 0.5) * (uv.x - 0.5) * u_vignette.x +\n"
"		                    (uv.y - 0.5) * (uv.y - 0.5));\n"
"		float fade = clamp(radius * u_vignette.y + u_vignette.z, 0.0, 1.0);\n"
"		color = mix(color, u_vignetteColor.rgb, fade * u_vignetteColor.a);\n"
"	}\n"

"	FRAGCOLOR(vec4(color, 1.0));\n"
"}\n"

;
