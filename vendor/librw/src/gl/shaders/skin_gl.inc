const char *skin_vert_src =
"uniform mat4 u_boneMatrices[64];\n"

"VSIN(ATTRIB_POS)	vec3 in_pos;\n"

"VSOUT vec4 v_color;\n"
"VSOUT vec2 v_tex0;\n"
"VSOUT float v_fog;\n"
"VSOUT vec3 v_worldPos;\n"
"VSOUT vec3 v_worldNormal;\n"

"void\n"
"main(void)\n"
"{\n"
"	vec3 SkinVertex = vec3(0.0, 0.0, 0.0);\n"
"	vec3 SkinNormal = vec3(0.0, 0.0, 0.0);\n"
"	for(int i = 0; i < 4; i++){\n"
"		SkinVertex += (u_boneMatrices[int(in_indices[i])] * vec4(in_pos, 1.0)).xyz * in_weights[i];\n"
"		SkinNormal += (mat3(u_boneMatrices[int(in_indices[i])]) * in_normal) * in_weights[i];\n"
"	}\n"

"	vec4 Vertex = u_world * vec4(SkinVertex, 1.0);\n"
"	gl_Position = u_proj * u_view * Vertex;\n"
"	vec3 Normal = mat3(u_world) * SkinNormal;\n"

"	v_tex0 = (u_uvMatrix * vec4(in_tex0, 0.0, 1.0)).xy;\n"

"	if(u_atmosphere.x > 0.5 || u_surfProps.w >= 1.5){\n"
"		v_color = BR_EvalDynamicVertexColor(in_color, u_matColor, Vertex.xyz, Normal);\n"
"	}else{\n"
"		v_color = in_color;\n"
"		v_color.rgb += u_ambLight.rgb*surfAmbient;\n"
"		v_color.rgb += DoDynamicLight(Vertex.xyz, Normal)*surfDiffuse;\n"
"		v_color = clamp(v_color, 0.0, 1.0);\n"
"		v_color *= u_matColor;\n"
"	}\n"

"	v_worldPos = Vertex.xyz;\n"
"	v_worldNormal = Normal;\n"
"	v_fog = DoFogEye(Vertex.xyz);\n"
"}\n"
;
