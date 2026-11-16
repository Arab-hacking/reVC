uniform sampler2D tex0;
uniform vec4 u_blurcolor;
uniform vec4 u_blurTexel;

FSIN vec4 v_color;
FSIN vec2 v_tex0;
FSIN float v_fog;

#define FXAA_SPAN_MAX 8.0
#define FXAA_REDUCE_MUL (1.0 / 8.0)
#define FXAA_REDUCE_MIN (1.0 / 128.0)

vec3
SampleBackbufferClamped(vec2 uv)
{
	vec2 minUV = vec2(0.5) * u_blurTexel.xy;
	vec2 maxUV = max(u_blurTexel.zw - minUV, minUV);
	vec2 c = clamp(uv, minUV, maxUV);
	return texture(tex0, vec2(c.x, 1.0 - c.y)).rgb;
}

// postprocessing/Fxaa.fs (Shaders For BLAYZ RUSSIA PC)
vec3
SampleBackbufferFXAA(vec2 posPos)
{
	vec2 rcpFrame = u_blurTexel.xy;
	vec3 rgbNW = SampleBackbufferClamped(posPos + vec2(-1.0, -1.0) * rcpFrame);
	vec3 rgbNE = SampleBackbufferClamped(posPos + vec2( 1.0, -1.0) * rcpFrame);
	vec3 rgbSW = SampleBackbufferClamped(posPos + vec2(-1.0,  1.0) * rcpFrame);
	vec3 rgbSE = SampleBackbufferClamped(posPos + vec2( 1.0,  1.0) * rcpFrame);
	vec3 rgbM  = SampleBackbufferClamped(posPos);

	vec3 luma = vec3(0.299, 0.587, 0.114);
	float lumaNW = dot(rgbNW, luma);
	float lumaNE = dot(rgbNE, luma);
	float lumaSW = dot(rgbSW, luma);
	float lumaSE = dot(rgbSE, luma);
	float lumaM  = dot(rgbM,  luma);

	float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
	float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
	if((lumaMax - lumaMin) < max(0.0312, lumaMax * 0.125))
		return rgbM;

	vec2 dir;
	dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
	dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

	float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUCE_MUL), FXAA_REDUCE_MIN);
	float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
	dir = min(vec2(FXAA_SPAN_MAX), max(vec2(-FXAA_SPAN_MAX), dir * rcpDirMin)) * rcpFrame;

	vec3 rgbA = 0.5 * (
		SampleBackbufferClamped(posPos + dir * (1.0 / 3.0 - 0.5)) +
		SampleBackbufferClamped(posPos + dir * (2.0 / 3.0 - 0.5)));
	vec3 rgbB = rgbA * 0.5 + 0.25 * (
		SampleBackbufferClamped(posPos + dir * -0.5) +
		SampleBackbufferClamped(posPos + dir *  0.5));
	float lumaB = dot(rgbB, luma);
	if(lumaB < lumaMin || lumaB > lumaMax)
		return rgbA;
	return rgbB;
}

vec3
ExtractHighlight(vec3 c)
{
	float peak = max(c.r, max(c.g, c.b));
	float soft = clamp(peak - 0.80, 0.0, 0.35);
	float w = max(peak - 0.88, 0.0) + (soft * soft) * 0.80;
	return c * (w / max(peak, 1.0e-4));
}

void
main(void)
{
	if(u_blurTexel.x > 0.0){
		vec3 scene = SampleBackbufferFXAA(v_tex0);

		// Subtle time-of-day tint without washing out ACES-tonemapped white cars
		float gradeWeight = clamp(u_blurcolor.a * 0.28, 0.0, 0.16);
		vec3 graded = clamp(scene * (1.0 + u_blurcolor.rgb * 0.35), 0.0, 1.0);
		scene = mix(scene, graded, gradeWeight);

		// 8-tap soft highlight bloom around sun glints, sky horizon, and streetlamps
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

		vec3 bloomTint = mix(vec3(1.03, 1.00, 0.95), 1.0 + u_blurcolor.rgb * 0.45, 0.30);
		vec3 composed = scene + bloom * bloomTint * 0.24;

		// postprocessing/Vignette.fs: subtle radial edge darkening
		vec2 screenUV = clamp(v_tex0 / max(u_blurTexel.zw, vec2(1.0e-4)), 0.0, 1.0) - vec2(0.5);
		float vig = 1.0 - smoothstep(0.38, 0.78, length(screenUV)) * 0.085;
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
