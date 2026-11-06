uniform sampler2D tex0;
uniform sampler2D tex1;
uniform vec4 u_lightMap;
uniform float u_brLightmap;

#define u_lm (vec4(u_lightMap.rgb, 0.0))

FSIN vec4 v_color;
FSIN vec2 v_tex0;
FSIN vec2 v_tex1;
FSIN float v_fog;
FSIN vec3 v_worldPos;
FSIN vec3 v_worldNormal;

void
main(void)
{
	vec4 t0 = texture(tex0, vec2(v_tex0.x, 1.0-v_tex0.y));
	vec4 t1 = texture(tex1, vec2(v_tex1.x, 1.0-v_tex1.y));

	vec4 surface = mix(t0, t0*t1*(1.0 + u_lm), t0.a*u_lightMap.a);
	surface.a = t0.a*v_color.a;
	DoAlphaTest(surface.a);

	float visibility = AtmosphereVisibility(v_worldPos, v_fog);
	vec4 color;
	color.rgb = ShadeWorldSurfaceEx(surface.rgb, v_color.rgb, v_worldPos, v_worldNormal, visibility, 0.0);
	color.a = surface.a;

	FRAGCOLOR(color);
}
