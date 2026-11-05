uniform sampler2D tex0;
uniform sampler2D tex1;
uniform vec4 u_reflProps; // x: Fresnel floor, y: lightStrength, z: shininess, w: specularity
uniform vec4 u_specDir[5];
uniform vec4 u_specColor[5];

FSIN vec4 v_color;
FSIN vec2 v_tex0;
FSIN vec3 v_worldPos;
FSIN vec3 v_worldNormal;
FSIN float v_fog;

vec3 DoDirLightSpec(vec3 Ldir, vec3 Lcol, vec3 N, vec3 V, float power)
{
	float lLen2 = dot(Ldir, Ldir);
	if(lLen2 < 1.0e-8)
		return vec3(0.0);
	vec3 L = -Ldir * inversesqrt(lLen2);
	if(dot(N, L) <= 0.0)
		return vec3(0.0);
	vec3 halfVec = V + L;
	float hLen2 = dot(halfVec, halfVec);
	if(hLen2 < 1.0e-8)
		return vec3(0.0);
	vec3 H = halfVec * inversesqrt(hLen2);
	return pow(clamp(dot(N, H), 0.0, 1.0), max(power, 1.0)) * Lcol;
}

void
main(void)
{
	vec4 pass1 = v_color * texture(tex0, vec2(v_tex0.x, 1.0 - v_tex0.y));
	float nLen2 = dot(v_worldNormal, v_worldNormal);
	vec3 N = nLen2 > 1.0e-8 ? v_worldNormal * inversesqrt(nLen2) : vec3(0.0, 0.0, 1.0);
	float selfShade;
	pass1.rgb += SunMaterialLighting(v_worldPos, N, selfShade);
	pass1.rgb *= 1.0 - selfShade;

	vec3 toEye = u_eye.xyz - v_worldPos;
	vec3 V = toEye * inversesqrt(max(dot(toEye, toEye), 1.0e-8));
	vec3 R = reflect(-V, N);

	// Seamless upper-hemisphere paraboloid projection matching the +Z EnvMapCam:
	// zenith -> (0.5, 0.5), horizon -> outer ring, continuous across R.z == 0
	// with realistic ground-horizon darkening below the beltline (R.z < 0).
	float rLen2 = dot(R, R);
	vec3 Rp = rLen2 > 1.0e-8 ? vec3(R.xy, abs(R.z)) * inversesqrt(rLen2) : vec3(0.0, 0.0, 1.0);
	vec2 envUV = (Rp.xy / (1.0 + Rp.z)) * 0.485 + 0.5;
	vec3 envmap = texture(tex1, vec2(envUV.x, 1.0 - envUV.y)).rgb;
	float horizonAtten = mix(0.42, 1.0, smoothstep(-0.45, 0.05, R.z));
	envmap *= horizonAtten;

	float ndotv = clamp(abs(dot(V, N)), 0.0, 1.0);
	float fresnel = pow(1.0 - ndotv, 3.5);
	float f0 = max(u_reflProps.x, 0.28);
	float surfaceMask = smoothstep(0.0, 0.05, pass1.a);
	float reflection = clamp(mix(fresnel, 1.0, f0) * u_reflProps.z, 0.0, 0.92) * surfaceMask;

	vec3 specLight = vec3(0.0);
	float specScale = u_reflProps.w * u_reflProps.y * surfaceMask;
	if(specScale > 0.0){
		for(int i = 0; i < 5; i++)
			specLight += DoDirLightSpec(u_specDir[i].xyz, u_specColor[i].rgb, N, V, u_specDir[i].w);
		specLight *= specScale;
	}

	float visibility = AtmosphereVisibility(v_worldPos, v_fog);
	vec3 baseLit = mix(u_fogColor.rgb, pass1.rgb, visibility);

	vec4 color;
	color.rgb = baseLit * pass1.a * (1.0 - reflection) + (envmap * reflection + specLight) * visibility;
	color.a = clamp(pass1.a + reflection * (1.0 - pass1.a), 0.0, 1.0);
	color.rgb = pow(max(color.rgb, vec3(0.0)), vec3(u_localInvGamma.x));

	DoAlphaTest(color.a);

	FRAGCOLOR(color);
}
