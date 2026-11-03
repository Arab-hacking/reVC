const char *matfx_env_vert_src =
"uniform vec4 u_colorClamp;\n"
"uniform vec4 u_envColor;\n"

"VSIN(ATTRIB_POS)	vec3 in_pos;\n"

"VSOUT vec4 v_color;\n"
"VSOUT vec4 v_envColor;\n"
"VSOUT vec2 v_tex0;\n"
"VSOUT float v_fog;\n"
"VSOUT vec3 v_worldPos;\n"
"VSOUT vec3 v_worldNormal;\n"

"void\n"
"main(void)\n"
"{\n"
"	vec4 Vertex = u_world * vec4(in_pos, 1.0);\n"
"	gl_Position = u_proj * u_view * Vertex;\n"
"	vec3 Normal = mat3(u_world) * in_normal;\n"

"	v_tex0 = (u_uvMatrix * vec4(in_tex0, 0.0, 1.0)).xy;\n"

"	v_color = in_color;\n"
"	v_color.rgb += u_ambLight.rgb*surfAmbient;\n"
"	v_color.rgb += DoDynamicLight(Vertex.xyz, Normal)*surfDiffuse;\n"
"	v_color = clamp(v_color, 0.0, 1.0);\n"
"	v_envColor = max(v_color, u_colorClamp) * u_envColor;\n"
"	v_color *= u_matColor;\n"

"	v_worldPos = Vertex.xyz;\n"
"	v_worldNormal = Normal;\n"
"	v_fog = DoFogEye(Vertex.xyz);\n"
"}\n"
;
const char *matfx_env_frag_src =
"uniform sampler2D tex0;\n"
"uniform sampler2D tex1;\n"
"uniform mat4 u_texMatrix;\n"

"uniform vec4 u_fxparams;\n"

"#define shininess (u_fxparams.x)\n"
"#define disableFBA (u_fxparams.y)\n"

"FSIN vec4 v_color;\n"
"FSIN vec4 v_envColor;\n"
"FSIN vec2 v_tex0;\n"
"FSIN float v_fog;\n"
"FSIN vec3 v_worldPos;\n"
"FSIN vec3 v_worldNormal;\n"

"void\n"
"main(void)\n"
"{\n"
"	vec4 pass1 = v_color;\n"
"	pass1 *= texture(tex0, vec2(v_tex0.x, 1.0-v_tex0.y));\n"

"	// Reflect the view direction in world space, then transform it using the\n"
"	// actual MatFX environment frame (including envMapFlipU). Unlike the old\n"
"	// normalize(refl.xy) projection this is defined for head-on reflections.\n"
"	vec3 N = normalize(v_worldNormal);\n"
"	vec3 V = normalize(u_eye.xyz - v_worldPos);\n"
"	vec3 refl = reflect(-V, N);\n"
"	vec2 envUV = (u_texMatrix * vec4(refl, 1.0)).xy;\n"
"	vec3 envmap = pow(texture(tex1, envUV).rgb, vec3(0.8));\n"
"	float fres = 1.0 - clamp(dot(V, N), 0.0, 1.0);\n"
"	fres = clamp(pow(fres, 3.5), 0.15, 0.7);\n"
"	// Respect MatFX's material tint / optional lighting and alpha blending.\n"
"	vec3 pass2 = envmap * v_envColor.rgb * fres * shininess;\n"

"	float selfShade;\n"
"	pass1.rgb += SunMaterialLighting(v_worldPos, N, selfShade);\n"
"	pass1.rgb *= 1.0 - selfShade;\n"

"	pass1.rgb = mix(u_fogColor.rgb, pass1.rgb, v_fog);\n"
"	pass2 = mix(vec3(0.0, 0.0, 0.0), pass2, v_fog);\n"

"	float fba = max(pass1.a, disableFBA);\n"
"	vec4 color;\n"
"	color.rgb = pass1.rgb*pass1.a + pass2*fba;\n"
"	color.a = pass1.a;\n"

"	// Apply gamma after fog and compositing; do not shade the fog colour.\n"
"	color.rgb = pow(max(color.rgb, vec3(0.0)), vec3(u_localInvGamma.x));\n"

"	DoAlphaTest(color.a);\n"

"	FRAGCOLOR(color);\n"
"}\n"
;
