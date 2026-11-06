VSIN(ATTRIB_POS)	vec3 in_pos;

VSOUT vec4 v_color;
VSOUT vec2 v_tex0;
VSOUT vec2 v_tex1;
VSOUT float v_fog;
VSOUT vec3 v_worldPos;
VSOUT vec3 v_worldNormal;

void
main(void)
{
	vec4 Vertex = u_world * vec4(in_pos, 1.0);
	gl_Position = u_proj * u_view * Vertex;
	vec3 Normal = mat3(u_world) * in_normal;

	v_tex0 = (u_uvMatrix * vec4(in_tex0, 0.0, 1.0)).xy;
	v_tex1 = in_tex1;

	if(u_atmosphere.x > 0.5){
		v_color = BR_EvalBuildingVertexColor(in_color, u_matColor, Vertex.xyz, Normal);
	}else{
		v_color = in_color;
		v_color.rgb += u_ambLight.rgb*surfAmbient;
		v_color.rgb += DoDynamicLight(Vertex.xyz, Normal)*surfDiffuse;
		v_color = clamp(v_color, 0.0, 1.0);
		v_color *= u_matColor;
	}

	v_worldPos = Vertex.xyz;
	v_worldNormal = Normal;
	v_fog = DoFog(gl_Position.w);
}
