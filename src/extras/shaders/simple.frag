uniform sampler2D tex0;

FSIN vec4 v_color;
FSIN vec2 v_tex0;
FSIN float v_fog;
FSIN vec3 v_worldPos;
FSIN vec3 v_worldNormal;

void
main(void)
{
	vec4 texCol = texture(tex0, vec2(v_tex0.x, 1.0-v_tex0.y));
	vec4 color;
	color.a = texCol.a * v_color.a;
	DoAlphaTest(color.a);

	float visibility = AtmosphereVisibility(v_worldPos, v_fog);
	color.rgb = ShadeWorldSurfaceEx(texCol.rgb, v_color.rgb, v_worldPos, v_worldNormal, visibility, 1.0);

	FRAGCOLOR(color);
}
