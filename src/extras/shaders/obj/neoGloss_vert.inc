const char *neoGloss_vert_src =
"VSIN(ATTRIB_POS)	vec3 in_pos;\n"

"VSOUT vec3 v_normal;\n"
"VSOUT vec3 v_light;\n"
"VSOUT vec2 v_tex0;\n"
"VSOUT float v_fog;\n"

"void\n"
"main(void)\n"
"{\n"
"	vec4 Vertex = u_world * vec4(in_pos, 1.0);\n"
"	// Paraboloid capture pass (client 0x42b527): while u_paraboloid.x > 0.5 the\n"
"	// scene is written with the polar projection instead of the camera, so the\n"
"	// env-map being rendered uses the same mapping the materials sample.\n"
"	gl_Position = u_paraboloid.x > 0.5 ? ParaboloidProjection(Vertex.xyz)\n"
"	                                  : u_proj * u_view * Vertex;\n"
"	vec3 Normal = normalize(mat3(u_world) * in_normal);\n"

"	v_tex0 = in_tex0;\n"

"	vec3 viewVec = normalize(u_eye.xyz - Vertex.xyz);\n"
"	vec3 Light = normalize(viewVec - u_lightDirection[0].xyz);\n"
"	v_normal = 0.5*(1.0 + Normal); // compress actual road normal\n"
"	v_light  = 0.5*(1.0 + Light);\n"

"	v_fog = DoFog(gl_Position.w);\n"
"}\n"
;
