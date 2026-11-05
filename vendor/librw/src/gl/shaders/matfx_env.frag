uniform sampler2D tex0;
uniform sampler2D tex1;
uniform mat4 u_texMatrix;

uniform vec4 u_fxparams;

#define shininess (u_fxparams.x)
#define disableFBA (u_fxparams.y)

FSIN vec4 v_color;
FSIN vec4 v_envColor;
FSIN vec2 v_tex0;
FSIN float v_fog;
FSIN vec3 v_worldPos;
FSIN vec3 v_worldNormal;

void
main(void)
{
	vec4 pass1 = v_color;
	pass1 *= texture(tex0, vec2(v_tex0.x, 1.0-v_tex0.y));
	DoAlphaTest(pass1.a);

	float hasVertNormal;
	vec3 N = ResolveWorldNormal(v_worldPos, v_worldNormal, hasVertNormal);
	vec3 V = normalize(u_eye.xyz - v_worldPos);
	vec3 refl = reflect(-V, N);
	vec2 envUV = (u_texMatrix * vec4(refl, 1.0)).xy;
	vec3 envmap = pow(max(texture(tex1, envUV).rgb, vec3(0.0)), vec3(0.9));
	float fres = 1.0 - clamp(dot(V, N), 0.0, 1.0);
	fres = clamp(pow(fres, 3.5), 0.08, 0.70);

	float shadowVis = u_atmosphere.x > 0.5 ? SampleSunShadow(v_worldPos, N) : 1.0;
	vec3 pass2 = envmap * v_envColor.rgb * fres * shininess * mix(0.55, 1.0, shadowVis);

	float visibility = AtmosphereVisibility(v_worldPos, v_fog);
	pass1.rgb = ShadeWorldSurface(pass1.rgb, v_worldPos, v_worldNormal, visibility);
	pass2 *= visibility;

	float fba = max(pass1.a, disableFBA);
	vec4 color;
	color.rgb = clamp(pass1.rgb*pass1.a + pass2*fba, 0.0, 1.0);
	color.a = pass1.a;

	FRAGCOLOR(color);
}
