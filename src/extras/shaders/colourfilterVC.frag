uniform sampler2D tex0;
uniform vec4 u_blurcolor;
uniform vec4 u_blurTexel;

FSIN vec4 v_color;
FSIN vec2 v_tex0;
FSIN float v_fog;

vec3
ExtractHighlight(vec3 c)
{
	float peak = max(c.r, max(c.g, c.b));
	float soft = clamp(peak - 0.78, 0.0, 0.40);
	float w = max(peak - 0.85, 0.0) + (soft * soft) * 0.85;
	return c * (w / max(peak, 1.0e-4));
}

vec3
SampleBackbufferClamped(vec2 uv)
{
	vec2 minUV = vec2(0.5) * u_blurTexel.xy;
	vec2 maxUV = max(u_blurTexel.zw - minUV, minUV);
	vec2 c = clamp(uv, minUV, maxUV);
	return texture(tex0, vec2(c.x, 1.0 - c.y)).rgb;
}

void
main(void)
{
	if(u_blurTexel.x > 0.0){
		vec3 scene = SampleBackbufferClamped(v_tex0);

		// Subtle time-of-day warmth without VC's 5-pass highlight blowout on white cars
		float gradeWeight = clamp(u_blurcolor.a * 0.35, 0.0, 0.22);
		vec3 graded = clamp(scene * (1.0 + u_blurcolor.rgb * 0.42), 0.0, 1.0);
		scene = mix(scene, graded, gradeWeight);

		// 8-tap golden highlight bloom around sun glints and sky highlights
		vec2 t1 = u_blurTexel.xy * 2.4;
		vec2 t2 = u_blurTexel.xy * 5.2;
		vec3 bloom = vec3(0.0);
		bloom += ExtractHighlight(SampleBackbufferClamped(v_tex0 + vec2( t1.x,  0.0)));
		bloom += ExtractHighlight(SampleBackbufferClamped(v_tex0 + vec2(-t1.x,  0.0)));
		bloom += ExtractHighlight(SampleBackbufferClamped(v_tex0 + vec2( 0.0,  t1.y)));
		bloom += ExtractHighlight(SampleBackbufferClamped(v_tex0 + vec2( 0.0, -t1.y)));
		bloom += ExtractHighlight(SampleBackbufferClamped(v_tex0 + vec2( t2.x,  t2.y))) * 0.75;
		bloom += ExtractHighlight(SampleBackbufferClamped(v_tex0 + vec2(-t2.x,  t2.y))) * 0.75;
		bloom += ExtractHighlight(SampleBackbufferClamped(v_tex0 + vec2( t2.x, -t2.y))) * 0.75;
		bloom += ExtractHighlight(SampleBackbufferClamped(v_tex0 + vec2(-t2.x, -t2.y))) * 0.75;
		bloom *= (1.0 / 7.0);

		vec3 bloomTint = mix(vec3(1.04, 1.00, 0.94), 1.0 + u_blurcolor.rgb * 0.5, 0.35);
		vec3 composed = scene + bloom * bloomTint * 0.28;

		// Subtle natural corner vignette centered on the active viewport
		vec2 screenUV = clamp(v_tex0 / max(u_blurTexel.zw, vec2(1.0e-4)), 0.0, 1.0) - vec2(0.5);
		float vig = 1.0 - smoothstep(0.32, 0.86, dot(screenUV, screenUV) * 1.55) * 0.075;
		composed = clamp(composed * vig, 0.0, 1.0);

		FRAGCOLOR(vec4(composed, 1.0));
		return;
	}

	float a = u_blurcolor.a;
	vec4 doublec = clamp(u_blurcolor*2.0, 0.0, 1.0);
	vec4 dst = texture(tex0, vec2(v_tex0.x, 1.0-v_tex0.y));
	vec4 prev = dst;
	for(int i = 0; i < 5; i++){
		vec4 tmp = dst*(1.0-a) + prev*doublec*a;
		tmp += prev*u_blurcolor;
		tmp += prev*u_blurcolor;
		prev = clamp(tmp, 0.0, 1.0);
	}
	vec4 color;
	color.rgb = prev.rgb;
	color.a = 1.0;

	FRAGCOLOR(color);
}
