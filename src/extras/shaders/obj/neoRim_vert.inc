const char *neoRim_vert_src =
"uniform vec3 u_viewVec;\n"
"uniform vec4 u_rampStart;\n"
"uniform vec4 u_rampEnd;\n"
"uniform vec3 u_rimData;\n"

"VSIN(ATTRIB_POS)	vec3 in_pos;\n"

"VSOUT vec4 v_color;\n"
"VSOUT vec2 v_tex0;\n"
"VSOUT float v_fog;\n"
"VSOUT vec3 v_worldPos;\n"
"VSOUT vec3 v_worldNormal;\n"

"vec3 DoRimLight(vec3 Normal)\n"
"{\n"
"	float f = u_rimData.x - u_rimData.y*dot(Normal, u_viewVec);\n"
"	float rim = clamp(clamp(f, 0.0, 1.0)*u_rimData.z, 0.0, 1.0);\n"
"	return mix(u_rampStart.rgb, u_rampEnd.rgb, rim);\n"
"}\n"

"void\n"
"main(void)\n"
"{\n"
"	vec4 Vertex = u_world * vec4(in_pos, 1.0);\n"
"	gl_Position = u_proj * u_view * Vertex;\n"
"	vec3 Normal = mat3(u_world) * in_normal;\n"

"	v_tex0 = (u_uvMatrix * vec4(in_tex0, 0.0, 1.0)).xy;\n"

"	if(u_atmosphere.x > 0.5 || u_surfProps.w >= 1.5){\n"
"		v_color = BR_EvalDynamicVertexColor(in_color, u_matColor, Vertex.xyz, Normal);\n"
"	}else{\n"
"		v_color = in_color;\n"
"		v_color.rgb += u_ambLight.rgb*surfAmbient;\n"
"		v_color.rgb += DoDynamicLight(Vertex.xyz, Normal)*surfDiffuse;\n"
"		v_color.rgb += DoRimLight(Normal);\n"
"		v_color = clamp(v_color, 0.0, 1.0);\n"
"		v_color *= u_matColor;\n"
"	}\n"

"	v_worldPos = Vertex.xyz;\n"
"	v_worldNormal = Normal;\n"
"	v_fog = DoFog(gl_Position.w);\n"
"}\n"
;
