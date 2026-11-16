uniform sampler2D tex0;
uniform vec3 u_contrastAdd;
uniform vec3 u_contrastMult;
// Posteffects/Vignette.fs from the client:
//   radius = sqrt(pow2(uv.x - 0.5) * params.x + pow2(uv.y - 0.5));
//   fade   = clamp(radius * params.y + params.z, 0.0, 1.0);
//   color  = mix(color, vignetteColor.rgb, fade * vignetteColor.a);
// u_vignetteColor.a == 0 keeps this a pure contrast step, which is what the
// port did before, so the default look is unchanged until something enables it.
uniform vec4 u_vignette;
uniform vec4 u_vignetteColor;

FSIN vec4 v_color;
FSIN vec2 v_tex0;
FSIN float v_fog;

void
main(void)
{
	vec2 uv = vec2(v_tex0.x, 1.0-v_tex0.y);
	vec4 dst = texture(tex0, uv);
	vec3 color = dst.rgb*u_contrastMult + u_contrastAdd;
	if(u_vignetteColor.a > 0.0){
		float radius = sqrt((uv.x - 0.5) * (uv.x - 0.5) * u_vignette.x +
		                    (uv.y - 0.5) * (uv.y - 0.5));
		float fade = clamp(radius * u_vignette.y + u_vignette.z, 0.0, 1.0);
		color = mix(color, u_vignetteColor.rgb, fade * u_vignetteColor.a);
	}

	FRAGCOLOR(vec4(color, 1.0));
}

