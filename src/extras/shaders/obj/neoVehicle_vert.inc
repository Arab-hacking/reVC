const char *neoVehicle_vert_src =
"uniform vec4 u_reflProps;\n"
"uniform vec4 u_specDir[5];\n"
"uniform vec4 u_specColor[5];\n"

"#define lightStrength (u_reflProps.y)	// speclight alpha\n"
"#define specularity (u_reflProps.w)\n"

"VSIN(ATTRIB_POS)	vec3 in_pos;\n"

"VSOUT vec4 v_color;\n"
"VSOUT vec3 v_specular;\n"
"VSOUT vec2 v_tex0;\n"
"VSOUT vec3 v_worldPos;\n"
"VSOUT vec3 v_worldNormal;\n"
"VSOUT float v_fog;\n"

"vec3 DoDirLightSpec(vec3 Ldir, vec3 Lcol, vec3 N, vec3 V, float power)\n"
"{\n"
"	return pow(clamp(dot(N, normalize(V + -Ldir)), 0.0, 1.0), power)*Lcol;\n"
"}\n"

"void\n"
"main(void)\n"
"{\n"
"	vec4 Vertex = u_world * vec4(in_pos, 1.0);\n"
"	gl_Position = u_proj * u_view * Vertex;\n"
"	vec3 Normal = normalize(mat3(u_world) * in_normal);\n"
"	vec3 viewVec = normalize(u_eye.xyz - Vertex.xyz);\n"

"	v_tex0 = in_tex0;\n"

"	v_color = in_color;\n"
"	v_color.rgb += u_ambLight.rgb*surfAmbient;\n"
"	v_color.rgb += DoDynamicLight(Vertex.xyz, Normal)*surfDiffuse*lightStrength;\n"
"	v_color = clamp(v_color, 0.0, 1.0);\n"
"	v_color *= u_matColor;\n"

"	// Reflect and evaluate Fresnel in the fragment shader, not at a triangle's\n"
"	// vertices. Only the specular light is still interpolated here.\n"
"	v_worldPos = Vertex.xyz;\n"
"	v_worldNormal = Normal;\n"
"	v_specular = vec3(0.0);\n"
"	for(int i = 0; i < 5; i++)\n"
"		v_specular += DoDirLightSpec(u_specDir[i].xyz, u_specColor[i].rgb, Normal, viewVec, u_specDir[i].w)*specularity*lightStrength;\n"

"	v_fog = DoFogEye(Vertex.xyz);\n"
"}\n"
;
