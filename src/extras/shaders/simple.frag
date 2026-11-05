uniform sampler2D tex0;

FSIN vec4 v_color;
FSIN vec2 v_tex0;
FSIN float v_fog;
FSIN vec3 v_worldPos;
FSIN vec3 v_worldNormal;

void
main(void)
{
	vec4 color = v_color*texture(tex0, vec2(v_tex0.x, 1.0-v_tex0.y));
	DoAlphaTest(color.a);
	float visibility = AtmosphereVisibility(v_worldPos, v_fog);
	color.rgb = ShadeWorldSurface(color.rgb, v_worldPos, v_worldNormal, visibility);

	FRAGCOLOR(color);
}
