uniform sampler2D tex0;

FSIN vec4 v_color;
FSIN vec2 v_tex0;
FSIN float v_fog;
#if defined(BRMATERIAL) || defined(SHADOW_DEPTH_PASS)
FSIN vec3 v_worldPos;
FSIN vec3 v_worldNormal;
#endif

void
main(void)
{
#ifdef SHADOW_DEPTH_PASS
#ifndef NO_ALPHATEST
	float a = v_color.a * texture(tex0, vec2(v_tex0.x, 1.0-v_tex0.y)).a;
	DoAlphaTest(a);
#endif
	FRAGCOLOR(vec4(1.0));
#else
	vec4 color = v_color*texture(tex0, vec2(v_tex0.x, 1.0-v_tex0.y));
	DoAlphaTest(color.a);
#ifdef BRMATERIAL
	float visibility = AtmosphereVisibility(v_worldPos, v_fog);
	color.rgb = ShadeWorldSurface(color.rgb, v_worldPos, v_worldNormal, visibility);
#else
	color.rgb = mix(u_fogColor.rgb, color.rgb, v_fog);
#endif
	FRAGCOLOR(color);
#endif
}
