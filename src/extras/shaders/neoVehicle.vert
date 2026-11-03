uniform vec4 u_reflProps;
uniform vec4 u_specDir[5];
uniform vec4 u_specColor[5];

#define lightStrength (u_reflProps.y)	// speclight alpha
#define specularity (u_reflProps.w)

VSIN(ATTRIB_POS)	vec3 in_pos;

VSOUT vec4 v_color;
VSOUT vec3 v_specular;
VSOUT vec2 v_tex0;
VSOUT vec3 v_worldPos;
VSOUT vec3 v_worldNormal;
VSOUT float v_fog;

vec3 DoDirLightSpec(vec3 Ldir, vec3 Lcol, vec3 N, vec3 V, float power)
{
	return pow(clamp(dot(N, normalize(V + -Ldir)), 0.0, 1.0), power)*Lcol;
}

void
main(void)
{
	vec4 Vertex = u_world * vec4(in_pos, 1.0);
	gl_Position = u_proj * u_view * Vertex;
	vec3 Normal = normalize(mat3(u_world) * in_normal);
	vec3 viewVec = normalize(u_eye.xyz - Vertex.xyz);

	v_tex0 = in_tex0;

	v_color = in_color;
	v_color.rgb += u_ambLight.rgb*surfAmbient;
	v_color.rgb += DoDynamicLight(Vertex.xyz, Normal)*surfDiffuse*lightStrength;
	v_color = clamp(v_color, 0.0, 1.0);
	v_color *= u_matColor;

	// Reflect and evaluate Fresnel in the fragment shader, not at a triangle's
	// vertices. Only the specular light is still interpolated here.
	v_worldPos = Vertex.xyz;
	v_worldNormal = Normal;
	v_specular = vec3(0.0);
	for(int i = 0; i < 5; i++)
		v_specular += DoDirLightSpec(u_specDir[i].xyz, u_specColor[i].rgb, Normal, viewVec, u_specDir[i].w)*specularity*lightStrength;

	v_fog = DoFogEye(Vertex.xyz);
}
