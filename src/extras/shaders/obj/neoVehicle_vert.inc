const char *neoVehicle_vert_src =
"VSIN(ATTRIB_POS)	vec3 in_pos;\n"

"VSOUT vec4 v_color;\n"
"VSOUT vec4 v_prelit;\n"
"VSOUT vec4 v_lightFog;\n"
"VSOUT vec3 v_pointSpot;\n"
"VSOUT vec2 v_tex0;\n"
"VSOUT vec3 v_worldPos;\n"
"VSOUT vec3 v_worldNormal;\n"

"void\n"
"main(void)\n"
"{\n"
"	vec4 Vertex = u_world * vec4(in_pos, 1.0);\n"
"	gl_Position = u_proj * u_view * Vertex;\n"
"	vec3 Normal = mat3(u_world) * in_normal;\n"

"	v_tex0 = (u_uvMatrix * vec4(in_tex0, 0.0, 1.0)).xy;\n"
"	v_color = u_matColor;\n"
"	v_prelit = in_color;\n"
"	v_lightFog = vec4(DoDynamicLight(Vertex.xyz, Normal) * surfDiffuse, DoFogEye(Vertex.xyz));\n"
"	v_pointSpot = DoPointSpotLight(Vertex.xyz, Normal) * surfDiffuse;\n"
"	v_worldPos = Vertex.xyz;\n"
"	v_worldNormal = Normal;\n"
"}\n"
;
