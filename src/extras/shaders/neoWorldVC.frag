uniform sampler2D tex0;
uniform sampler2D tex1;

uniform vec4 u_lightMap;
uniform float u_brLightmap; // only previously marked UV2 textures use this alternate alpha mix

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

	vec4 color;
	vec3 nativeLight = t0.rgb*v_color.rgb*(1.0 + u_lightMap.rgb*(t1.rgb-1.0));
	float brWeight = clamp(t1.a*u_lightMap.x, 0.0, 1.0) * u_brLightmap;
	vec3 baseLit = mix(nativeLight, t0.rgb*v_color.rgb, u_brLightmap);
	color.a = v_color.a*t0.a*u_lightMap.a;
	DoAlphaTest(color.a);

	float visibility = AtmosphereVisibility(v_worldPos, v_fog);
	vec3 worldShaded = ShadeWorldSurface(baseLit, v_worldPos, v_worldNormal, visibility);
	vec3 bakedGlow = mix(u_fogColor.rgb, clamp(t0.rgb*t1.rgb*2.6, 0.0, 1.5), visibility);
	color.rgb = clamp(mix(worldShaded, max(worldShaded, bakedGlow), brWeight), 0.0, 1.0);

	FRAGCOLOR(color);
}
