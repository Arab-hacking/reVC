const char *neoVehicle_vert_src =
"uniform vec4 u_reflProps;\n"

"#define lightStrength (u_reflProps.y)	// speclight alpha\n"

"VSIN(ATTRIB_POS)	vec3 in_pos;\n"

"VSOUT vec4 v_color;\n"
"VSOUT vec2 v_tex0;\n"
"VSOUT vec3 v_worldPos;\n"
"VSOUT vec3 v_worldNormal;\n"
"VSOUT float v_fog;\n"

"void\n"
"main(void)\n"
"{\n"
"	vec4 Vertex = u_world * vec4(in_pos, 1.0);\n"
"	gl_Position = u_proj * u_view * Vertex;\n"
"	vec3 Normal = normalize(mat3(u_world) * in_normal);\n"

"	v_tex0 = in_tex0;\n"

"	v_color = in_color;\n"
"	v_color.rgb += u_ambLight.rgb*surfAmbient;\n"
"	v_color.rgb += DoDynamicLight(Vertex.xyz, Normal)*surfDiffuse*lightStrength;\n"
"	v_color = clamp(v_color, 0.0, 1.0);\n"
"	v_color *= u_matColor;\n"

"	v_worldPos = Vertex.xyz;\n"
"	v_worldNormal = Normal;\n"
"	v_fog = DoFogEye(Vertex.xyz);\n"
"}\n"
;
