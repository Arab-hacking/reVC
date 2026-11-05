uniform vec4 u_reflProps;

#define lightStrength (u_reflProps.y)	// speclight alpha

VSIN(ATTRIB_POS)	vec3 in_pos;

VSOUT vec4 v_color;
VSOUT vec2 v_tex0;
VSOUT vec3 v_worldPos;
VSOUT vec3 v_worldNormal;
VSOUT float v_fog;

void
main(void)
{
	vec4 Vertex = u_world * vec4(in_pos, 1.0);
	gl_Position = u_proj * u_view * Vertex;
	vec3 Normal = normalize(mat3(u_world) * in_normal);

	v_tex0 = in_tex0;

	v_color = in_color;
	v_color.rgb += u_ambLight.rgb*surfAmbient;
	v_color.rgb += DoDynamicLight(Vertex.xyz, Normal)*surfDiffuse*lightStrength;
	v_color = clamp(v_color, 0.0, 1.0);
	v_color *= u_matColor;

	v_worldPos = Vertex.xyz;
	v_worldNormal = Normal;
	v_fog = DoFogEye(Vertex.xyz);
}
