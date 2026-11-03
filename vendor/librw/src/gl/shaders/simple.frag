uniform sampler2D tex0;

FSIN vec4 v_color;
FSIN vec2 v_tex0;
FSIN float v_fog;
#ifdef BRMATERIAL
FSIN vec3 v_worldPos;
FSIN vec3 v_worldNormal;
#endif

void
main(void)
{
	vec4 color = v_color*texture(tex0, vec2(v_tex0.x, 1.0-v_tex0.y));
#ifdef BRMATERIAL
	float selfShade;
	color.rgb += SunMaterialLighting(v_worldPos, v_worldNormal, selfShade);
	color.rgb *= 1.0 - selfShade;
#endif
	color.rgb = mix(u_fogColor.rgb, color.rgb, v_fog);
#ifdef BRMATERIAL
	color.rgb = pow(color.rgb, vec3(u_localInvGamma.x));
#endif
	DoAlphaTest(color.a);
	FRAGCOLOR(color);
}
