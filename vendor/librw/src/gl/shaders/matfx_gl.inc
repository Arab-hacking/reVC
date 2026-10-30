const char *matfx_env_vert_src =
"uniform mat4 u_texMatrix;\n"
"uniform vec4 u_colorClamp;\n"
"uniform vec4 u_envColor;\n"

"VSIN(ATTRIB_POS)	vec3 in_pos;\n"

"VSOUT vec4 v_color;\n"
"VSOUT vec4 v_envColor;\n"
"VSOUT vec2 v_tex0;\n"
"VSOUT vec2 v_tex1;\n"
"VSOUT float v_fog;\n"
"VSOUT vec3 v_worldPos;\n"
"VSOUT vec3 v_worldNormal;\n"
"VSOUT float v_diffuse;\n"

"void\n"
"main(void)\n"
"{\n"
"	vec4 Vertex = u_world * vec4(in_pos, 1.0);\n"
"	gl_Position = u_proj * u_view * Vertex;\n"
"	vec3 Normal = mat3(u_world) * in_normal;\n"

"	v_tex0 = (u_uvMatrix * vec4(in_tex0, 0.0, 1.0)).xy;\n"
"	v_tex1 = (u_texMatrix * vec4(Normal, 1.0)).xy;\n"

"	v_color = in_color;\n"
"	v_color.rgb += u_ambLight.rgb*surfAmbient;\n"
"	v_color.rgb += DoDynamicLight(Vertex.xyz, Normal)*surfDiffuse;\n"
"	v_color = clamp(v_color, 0.0, 1.0);\n"
"	v_envColor = max(v_color, u_colorClamp) * u_envColor;\n"
"	v_color *= u_matColor;\n"

"	v_worldPos = Vertex.xyz;\n"
"	v_worldNormal = normalize(Normal);\n"
"	v_diffuse = max(dot(normalize(Normal), u_sunDir.xyz), 0.0);\n"
"	v_fog = DoFogEye(Vertex.xyz);\n"
"}\n"
;
const char *matfx_env_frag_src =
"uniform sampler2D tex0;\n"
"uniform sampler2D tex1;\n"

"uniform vec4 u_fxparams;\n"

"#define shininess (u_fxparams.x)\n"
"#define disableFBA (u_fxparams.y)\n"

"FSIN vec4 v_color;\n"
"FSIN vec4 v_envColor;\n"
"FSIN vec2 v_tex0;\n"
"FSIN vec2 v_tex1;\n"
"FSIN float v_fog;\n"
"FSIN vec3 v_worldPos;\n"
"FSIN vec3 v_worldNormal;\n"
"FSIN float v_diffuse;\n"

"void\n"
"main(void)\n"
"{\n"
"	vec4 pass1 = v_color;\n"
"	pass1 *= texture(tex0, vec2(v_tex0.x, 1.0-v_tex0.y));\n"

"	// Black Russia envmap: fresnel-weighted reflection probe\n"
"	vec3 viewVec = normalize(v_worldPos - u_eye.xyz);\n"
"	vec3 refl = reflect(viewVec, v_worldNormal);\n"
"	vec2 ReflPos = normalize(refl.xy) * (refl.z * 0.5 + 0.5);\n"
"	ReflPos = ReflPos * vec2(0.5, 0.5) + vec2(0.5, 0.5);\n"
"	vec3 envmap = pow(texture(tex1, ReflPos).rgb, vec3(0.8));\n"
"	float fres = 1.0 - abs(dot(-viewVec, v_worldNormal));\n"
"	fres = clamp(pow(fres, 3.5), 0.15, 0.7);\n"
"	vec3 pass2 = envmap * fres * shininess;\n"

"	// per-pixel sun diffuse + blinn-phong specular on the base layer\n"
"	vec3 N = normalize(v_worldNormal);\n"
"	vec3 viewDir = normalize(u_eye.xyz - v_worldPos);\n"
"	vec3 halfwayDir = normalize(u_sunDir.xyz + viewDir);\n"
"	vec3 sunLightingColor = vec3(0.2, 0.2, 0.2);\n"
"	float diffuseCoef = max(dot(N, u_sunDir.xyz), 0.0);\n"
"	vec3 diffuseVec = diffuseCoef * v_diffuse * sunLightingColor;\n"
"	float spec = pow(max(dot(N, halfwayDir), 0.0), 50.0);\n"
"	pass1.rgb += diffuseVec + sunLightingColor * spec * 0.4;\n"

"	pass1.rgb = mix(u_fogColor.rgb, pass1.rgb, v_fog);\n"
"	pass2 = mix(vec3(0.0, 0.0, 0.0), pass2, v_fog);\n"

"	float fba = max(pass1.a, disableFBA);\n"
"	vec4 color;\n"
"	color.rgb = pass1.rgb*pass1.a + pass2*fba;\n"
"	color.a = pass1.a;\n"

"	// silhouette shadow fallback + gamma, as in the client\n"
"	float brShadow = 0.3 * pow(1.0 - max(v_diffuse, 0.0), 24.0);\n"
"	color.rgb *= (1.0 - brShadow);\n"
"	color.rgb = pow(color.rgb, vec3(u_localInvGamma.x));\n"

"	DoAlphaTest(color.a);\n"

"	FRAGCOLOR(color);\n"
"}\n"
;
