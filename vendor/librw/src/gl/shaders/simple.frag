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
	vec4 texCol = texture(tex0, vec2(v_tex0.x, 1.0-v_tex0.y));
	vec4 color = vec4(texCol.rgb * v_color.rgb, texCol.a * v_color.a);
	DoAlphaTest(color.a);
#ifdef BRMATERIAL
	float visibility = AtmosphereVisibility(v_worldPos, v_fog);
#ifdef BRSKIN
	color.rgb = ShadeWorldSurfaceEx(texCol.rgb, v_color.rgb, v_worldPos, v_worldNormal, visibility, 1.0);
#else
	color.rgb = ShadeWorldSurfaceEx(texCol.rgb, v_color.rgb, v_worldPos, v_worldNormal, visibility, 0.0);
#endif
#else
	color.rgb = mix(u_fogColor.rgb, color.rgb, v_fog);
#endif
	FRAGCOLOR(color);
#endif
}
