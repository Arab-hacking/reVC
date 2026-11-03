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

	// Reflect the view direction in world space, then transform it using the
	// actual MatFX environment frame (including envMapFlipU). Unlike the old
	// normalize(refl.xy) projection this is defined for head-on reflections.
	vec3 N = normalize(v_worldNormal);
	vec3 V = normalize(u_eye.xyz - v_worldPos);
	vec3 refl = reflect(-V, N);
	vec2 envUV = (u_texMatrix * vec4(refl, 1.0)).xy;
	vec3 envmap = pow(texture(tex1, envUV).rgb, vec3(0.8));
	float fres = 1.0 - clamp(dot(V, N), 0.0, 1.0);
	fres = clamp(pow(fres, 3.5), 0.15, 0.7);
	// Respect MatFX's material tint / optional lighting and alpha blending.
	vec3 pass2 = envmap * v_envColor.rgb * fres * shininess;

	float selfShade;
	pass1.rgb += SunMaterialLighting(v_worldPos, N, selfShade);
	pass1.rgb *= 1.0 - selfShade;

	pass1.rgb = mix(u_fogColor.rgb, pass1.rgb, v_fog);
	pass2 = mix(vec3(0.0, 0.0, 0.0), pass2, v_fog);

	float fba = max(pass1.a, disableFBA);
	vec4 color;
	color.rgb = pass1.rgb*pass1.a + pass2*fba;
	color.a = pass1.a;

	// Apply gamma after fog and compositing; do not shade the fog colour.
	color.rgb = pow(max(color.rgb, vec3(0.0)), vec3(u_localInvGamma.x));

	DoAlphaTest(color.a);

	FRAGCOLOR(color);
}
