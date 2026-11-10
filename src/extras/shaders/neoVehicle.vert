VSIN(ATTRIB_POS)	vec3 in_pos;

VSOUT vec4 v_color;
VSOUT vec4 v_prelit;
VSOUT vec4 v_lightFog;
VSOUT vec3 v_pointSpot;
VSOUT vec2 v_tex0;
VSOUT vec3 v_worldPos;
VSOUT vec3 v_worldNormal;

void
main(void)
{
	vec4 Vertex = u_world * vec4(in_pos, 1.0);
	// Paraboloid capture pass (client 0x42b527): while u_paraboloid.x > 0.5 the
	// scene is written with the polar projection instead of the camera, so the
	// env-map being rendered uses the same mapping the materials sample.
	gl_Position = u_paraboloid.x > 0.5 ? ParaboloidProjection(Vertex.xyz)
	                                  : u_proj * u_view * Vertex;
	vec3 Normal = mat3(u_world) * in_normal;

	v_tex0 = (u_uvMatrix * vec4(in_tex0, 0.0, 1.0)).xy;
	v_color = u_matColor;
	v_prelit = in_color;
	v_lightFog = vec4(DoDynamicLight(Vertex.xyz, Normal) * surfDiffuse, DoFogEye(Vertex.xyz));
	v_pointSpot = DoPointSpotLight(Vertex.xyz, Normal) * surfDiffuse;
	v_worldPos = Vertex.xyz;
	v_worldNormal = Normal;
}
