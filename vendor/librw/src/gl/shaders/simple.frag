uniform sampler2D tex0;

FSIN vec4 v_color;
FSIN vec2 v_tex0;
FSIN float v_fog;
#ifdef BRMATERIAL
FSIN vec3 v_worldPos;
FSIN vec3 v_worldNormal;
FSIN float v_diffuse;
#endif

void
main(void)
{
	vec4 color = v_color*texture(tex0, vec2(v_tex0.x, 1.0-v_tex0.y));
#ifdef BRMATERIAL
	// per-pixel sun diffuse + blinn-phong specular (Black Russia material model)
	vec3 N = normalize(v_worldNormal);
	vec3 viewDir = normalize(u_eye.xyz - v_worldPos);
	vec3 halfwayDir = normalize(u_sunDir.xyz + viewDir);
	vec3 sunLightingColor = vec3(0.2, 0.2, 0.2);
	float diffuseCoef = max(dot(N, u_sunDir.xyz), 0.0);
	vec3 diffuseVec = diffuseCoef * v_diffuse * sunLightingColor;
	float spec = pow(max(dot(N, halfwayDir), 0.0), 50.0);
	vec3 specular = sunLightingColor * spec * 0.4;
	color.rgb += diffuseVec + specular;
	// silhouette shadow fallback, same curve as the client
	float brShadow = 0.3 * pow(1.0 - max(v_diffuse, 0.0), 24.0);
	color.rgb *= (1.0 - brShadow);
#endif
	color.rgb = mix(u_fogColor.rgb, color.rgb, v_fog);
#ifdef BRMATERIAL
	color.rgb = pow(color.rgb, vec3(u_localInvGamma.x));
#endif
	DoAlphaTest(color.a);
	FRAGCOLOR(color);
}
