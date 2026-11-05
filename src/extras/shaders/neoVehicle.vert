VSIN(ATTRIB_POS)	vec3 in_pos;

VSOUT vec4 v_color;
VSOUT vec4 v_prelit;
VSOUT vec4 v_lightFog;
VSOUT vec2 v_tex0;
VSOUT vec3 v_worldPos;
VSOUT vec3 v_worldNormal;

void
main(void)
{
	vec4 Vertex = u_world * vec4(in_pos, 1.0);
	gl_Position = u_proj * u_view * Vertex;
	vec3 Normal = mat3(u_world) * in_normal;

	v_tex0 = in_tex0;
	v_color = u_matColor;
	v_prelit = in_color;
	v_lightFog = vec4(DoDynamicLight(Vertex.xyz, Normal) * surfDiffuse, DoFogEye(Vertex.xyz));
	v_worldPos = Vertex.xyz;
	v_worldNormal = Normal;
}
