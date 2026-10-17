uniform sampler2D tex0;
uniform sampler2D tex1;

uniform vec4 u_fxparams;

#define shininess (u_fxparams.x)
#define disableFBA (u_fxparams.y)

FSIN vec4 v_color;
FSIN vec4 v_envColor;
FSIN vec2 v_tex0;
FSIN vec2 v_tex1;
FSIN float v_fog;
FSIN vec3 v_worldPos;
FSIN vec3 v_worldNormal;
FSIN float v_diffuse;

void
main(void)
{
	vec4 pass1 = v_color;
	pass1 *= texture(tex0, vec2(v_tex0.x, 1.0-v_tex0.y));

	// Black Russia envmap: fresnel-weighted reflection probe
	vec3 viewVec = normalize(v_worldPos - u_eye.xyz);
	vec3 refl = reflect(viewVec, v_worldNormal);
	vec2 ReflPos = normalize(refl.xy) * (refl.z * 0.5 + 0.5);
	ReflPos = ReflPos * vec2(0.5, 0.5) + vec2(0.5, 0.5);
	vec3 envmap = pow(texture(tex1, ReflPos).rgb, vec3(0.8));
	float fres = 1.0 - abs(dot(-viewVec, v_worldNormal));
	fres = clamp(pow(fres, 3.5), 0.15, 0.7);
	vec3 pass2 = envmap * fres * shininess;

	// per-pixel sun diffuse + blinn-phong specular on the base layer
	vec3 N = normalize(v_worldNormal);
	vec3 viewDir = normalize(u_eye.xyz - v_worldPos);
	vec3 halfwayDir = normalize(u_sunDir.xyz + viewDir);
	vec3 sunLightingColor = vec3(0.2, 0.2, 0.2);
	float diffuseCoef = max(dot(N, u_sunDir.xyz), 0.0);
	vec3 diffuseVec = diffuseCoef * v_diffuse * sunLightingColor;
	float spec = pow(max(dot(N, halfwayDir), 0.0), 50.0);
	pass1.rgb += diffuseVec + sunLightingColor * spec * 0.4;

	pass1.rgb = mix(u_fogColor.rgb, pass1.rgb, v_fog);
	pass2 = mix(vec3(0.0, 0.0, 0.0), pass2, v_fog);

	float fba = max(pass1.a, disableFBA);
	vec4 color;
	color.rgb = pass1.rgb*pass1.a + pass2*fba;
	color.a = pass1.a;

	// silhouette shadow fallback + gamma, as in the client
	float brShadow = 0.3 * pow(1.0 - max(v_diffuse, 0.0), 24.0);
	color.rgb *= (1.0 - brShadow);
	color.rgb = pow(color.rgb, vec3(u_localInvGamma.x));

	DoAlphaTest(color.a);

	FRAGCOLOR(color);
}
