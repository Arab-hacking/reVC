const char *neoVehicle_frag_src =
"uniform sampler2D tex0;\n"
"uniform sampler2D tex1;\n"
"uniform vec4 u_reflProps; // x: Fresnel floor, z: material shininess\n"

"FSIN vec4 v_color;\n"
"FSIN vec3 v_specular;\n"
"FSIN vec2 v_tex0;\n"
"FSIN vec3 v_worldPos;\n"
"FSIN vec3 v_worldNormal;\n"
"FSIN float v_fog;\n"

"void\n"
"main(void)\n"
"{\n"
"	vec4 pass1 = v_color*texture(tex0, vec2(v_tex0.x, 1.0-v_tex0.y));\n"
"	vec3 N = normalize(v_worldNormal);\n"
"	float selfShade;\n"
"	pass1.rgb += SunMaterialLighting(v_worldPos, N, selfShade);\n"
"	pass1.rgb *= 1.0 - selfShade;\n"
"	vec3 V = normalize(u_eye.xyz - v_worldPos);\n"
"	vec3 R = reflect(-V, N);\n"
"	vec2 envUV = R.xy * 0.5 + 0.5;\n"
"	vec3 envmap = texture(tex1, vec2(envUV.x, 1.0-envUV.y)).rgb;\n"
"	float grazing = 1.0 - clamp(dot(V, N), 0.0, 1.0);\n"
"	float schlick = grazing*grazing*grazing*grazing*grazing;\n"
"	float reflection = clamp(mix(schlick, 1.0, u_reflProps.x)*u_reflProps.z, 0.0, 1.0);\n"
"	pass1.rgb = mix(pass1.rgb, envmap, reflection);\n"
"	float visibility = AtmosphereVisibility(v_worldPos, v_fog);\n"
"	pass1.rgb = mix(u_fogColor.rgb, pass1.rgb, visibility);\n"

"	vec3 pass2 = v_specular * visibility;\n"

"	vec4 color;\n"
"	color.rgb = pass1.rgb*pass1.a + pass2;\n"
"	color.a = pass1.a;\n"

"	DoAlphaTest(color.a);\n"

"	FRAGCOLOR(color);\n"
"}\n"
;
