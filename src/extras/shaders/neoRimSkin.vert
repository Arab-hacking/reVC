uniform vec3 u_viewVec;
uniform vec4 u_rampStart;
uniform vec4 u_rampEnd;
uniform vec3 u_rimData;

VSIN(ATTRIB_POS)	vec3 in_pos;

uniform vec4 u_boneMatrices[64*3];

VSOUT vec4 v_color;
VSOUT vec2 v_tex0;
VSOUT float v_fog;
VSOUT vec3 v_worldPos;
VSOUT vec3 v_worldNormal;

vec3 DoRimLight(vec3 Normal)
{
	float f = u_rimData.x - u_rimData.y*dot(Normal, u_viewVec);
	float rim = clamp(clamp(f, 0.0, 1.0)*u_rimData.z, 0.0, 1.0);
	return mix(u_rampStart.rgb, u_rampEnd.rgb, rim);
}

void
main(void)
{
	vec3 SkinVertex = vec3(0.0, 0.0, 0.0);
	vec3 SkinNormal = vec3(0.0, 0.0, 0.0);
	for(int i = 0; i < 4; i++){
		float w = in_weights[i];
		if(w > 0.0){
			int bone = int(in_indices[i]) * 3;
			mat4 m = mat4(
				u_boneMatrices[bone + 0],
				u_boneMatrices[bone + 1],
				u_boneMatrices[bone + 2],
				vec4(0.0, 0.0, 0.0, 1.0)
			);
			SkinVertex += (vec4(in_pos, 1.0) * m).xyz * w;
			SkinNormal += (vec4(in_normal, 0.0) * m).xyz * w;
		}
	}

	vec4 Vertex = u_world * vec4(SkinVertex, 1.0);
	gl_Position = u_proj * u_view * Vertex;
	vec3 Normal = mat3(u_world) * SkinNormal;

	v_tex0 = (u_uvMatrix * vec4(in_tex0, 0.0, 1.0)).xy;

	if(u_atmosphere.x > 0.5){
		v_color = BR_EvalDynamicVertexColor(in_color, u_matColor, Vertex.xyz, Normal);
	}else{
		v_color = in_color;
		v_color.rgb += u_ambLight.rgb*surfAmbient;
		v_color.rgb += DoDynamicLight(Vertex.xyz, Normal)*surfDiffuse;
		v_color.rgb += DoRimLight(Normal);
		v_color = clamp(v_color, 0.0, 1.0);
		v_color *= u_matColor;
	}

	v_worldPos = Vertex.xyz;
	v_worldNormal = Normal;
	v_fog = DoFog(gl_Position.w);
}
