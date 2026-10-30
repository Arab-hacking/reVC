uniform sampler2D tex0;
uniform sampler2D tex1;

uniform vec4 u_lightMap;
uniform float u_brLightmap; // only previously marked UV2 textures use this alternate alpha mix

FSIN vec4 v_color;
FSIN vec2 v_tex0;
FSIN vec2 v_tex1;
FSIN float v_fog;

void
main(void)
{
	vec4 t0 = texture(tex0, vec2(v_tex0.x, 1.0-v_tex0.y));
	vec4 t1 = texture(tex1, vec2(v_tex1.x, 1.0-v_tex1.y));

	vec4 color;
	vec3 nativeLight = t0.rgb*v_color.rgb*(1.0 + u_lightMap.rgb*(t1.rgb-1.0));
	// BR tex1 is a lightmap in UV2, mixed by its ALPHA; the lit texture
	// contributes up to three times its albedo. Do not apply this to native
	// MatFX DUAL detail textures or unmarked model materials.
	vec3 brLight = mix(t0.rgb*v_color.rgb, t0.rgb*t1.rgb*3.0,
	                   clamp(t1.a*u_lightMap.x, 0.0, 1.0));
	color.rgb = mix(nativeLight, brLight, u_brLightmap);
	color.a = v_color.a*t0.a*u_lightMap.a;

	color.rgb = mix(u_fogColor.rgb, color.rgb, v_fog);
	DoAlphaTest(color.a);

	FRAGCOLOR(color);
}

