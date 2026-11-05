uniform sampler2D tex0;
uniform sampler2D tex1;
uniform vec4 u_reflProps; // x: Fresnel floor, z: material shininess

FSIN vec4 v_color;
FSIN vec3 v_specular;
FSIN vec2 v_tex0;
FSIN vec3 v_worldPos;
FSIN vec3 v_worldNormal;
FSIN float v_fog;

void
main(void)
{
	vec4 pass1 = v_color*texture(tex0, vec2(v_tex0.x, 1.0-v_tex0.y));
	vec3 N = normalize(v_worldNormal);
	float selfShade;
	pass1.rgb += SunMaterialLighting(v_worldPos, N, selfShade);
	pass1.rgb *= 1.0 - selfShade;
	vec3 V = normalize(u_eye.xyz - v_worldPos);
	vec3 R = reflect(-V, N);
	vec2 envUV = R.xy * 0.5 + 0.5;
	vec3 envmap = texture(tex1, vec2(envUV.x, 1.0-envUV.y)).rgb;
	float grazing = 1.0 - clamp(dot(V, N), 0.0, 1.0);
	float schlick = grazing*grazing*grazing*grazing*grazing;
	float reflection = clamp(mix(schlick, 1.0, u_reflProps.x)*u_reflProps.z, 0.0, 1.0);
	pass1.rgb = mix(pass1.rgb, envmap, reflection);
	float visibility = AtmosphereVisibility(v_worldPos, v_fog);
	pass1.rgb = mix(u_fogColor.rgb, pass1.rgb, visibility);

	vec3 pass2 = v_specular * visibility;

	vec4 color;
	color.rgb = pass1.rgb*pass1.a + pass2;
	color.a = pass1.a;

	DoAlphaTest(color.a);

	FRAGCOLOR(color);
}
