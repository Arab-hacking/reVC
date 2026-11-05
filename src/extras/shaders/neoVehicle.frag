uniform sampler2D tex0;
uniform sampler2D tex1;
uniform vec4 u_reflProps;
uniform vec4 u_specDir[5];
uniform vec4 u_specColor[5];
uniform vec4 u_surfProps;
uniform vec4 u_ambLight;

FSIN vec4 v_color;
FSIN vec4 v_prelit;
FSIN vec4 v_lightFog;
FSIN vec2 v_tex0;
FSIN vec3 v_worldPos;
FSIN vec3 v_worldNormal;

float
DoDirLightSpec(vec3 N, vec3 V, vec3 L, float power)
{
	vec3 H = normalize(V + L);
	return pow(clamp(dot(N, H), 0.0, 1.0), power);
}

void
main(void)
{
	vec4 texSample = texture(tex0, vec2(v_tex0.x, 1.0 - v_tex0.y));
	float alpha = clamp(v_color.a * texSample.a, 0.0, 1.0);
	DoAlphaTest(alpha);

	vec3 N = normalize(v_worldNormal);
	vec3 toEye = u_eye.xyz - v_worldPos;
	vec3 V = toEye * inversesqrt(max(dot(toEye, toEye), 1.0e-8));
	float visibility = AtmosphereVisibility(v_worldPos, v_lightFog.w);

	// Legacy fallback when GL3Atmosphere is disabled
	if(u_atmosphere.x <= 0.5){
		vec3 legacyVert = clamp(v_prelit.rgb + u_ambLight.rgb * u_surfProps.x + v_lightFog.rgb, 0.0, 1.4);
		vec3 pass1 = legacyVert * v_color.rgb * texSample.rgb;
		vec3 R0 = reflect(-V, N);
		vec3 Rp0 = normalize(vec3(R0.xy, abs(R0.z) + 1.0e-5));
		vec2 envUV0 = (Rp0.xy / (1.0 + Rp0.z)) * 0.485 + 0.5;
		vec3 env0 = texture(tex1, vec2(envUV0.x, 1.0 - envUV0.y)).rgb;
		float f0 = pow(1.0 - clamp(abs(dot(V, N)), 0.0, 1.0), 3.0);
		float rw0 = clamp(mix(u_reflProps.x, u_reflProps.y, f0) * u_reflProps.z, 0.0, 0.85);
		vec3 spec0 = vec3(0.0);
		for(int i = 0; i < 5; i++)
			spec0 += DoDirLightSpec(N, V, -u_specDir[i].xyz, 32.0) * u_specColor[i].rgb * u_reflProps.w;
		vec4 legacyOut;
		legacyOut.rgb = mix(u_fogColor.rgb, pass1, visibility) * alpha * (1.0 - rw0)
		              + (env0 * rw0 + spec0) * visibility;
		legacyOut.a = clamp(alpha + rw0 * (1.0 - alpha), 0.0, 1.0);
		FRAGCOLOR(legacyOut);
		return;
	}

	// Base paint / trim / tire albedo without crushing PRELIT==0 vehicle meshes
	vec3 albedoSrgb = v_color.rgb * texSample.rgb;
	float hasPrelit = step(0.001, dot(v_prelit.rgb, v_prelit.rgb));
	albedoSrgb *= mix(vec3(1.0), clamp(v_prelit.rgb * 1.15, 0.48, 1.0), 0.32 * hasPrelit);

	// Suppress glossy clearcoat & sun specular on dark textured rubber tires / black grilles,
	// while preserving glossy black body paint (untextured or bright remap) and tinted glass (alpha < 0.92).
	float isOpaque = step(0.92, alpha);
	float hasTex = step(0.5, u_surfProps.w);
	float texPeak = max(texSample.r, max(texSample.g, texSample.b));
	float darkTrimMask = mix(1.0, smoothstep(0.11, 0.35, texPeak), isOpaque * hasTex);

	vec3 L = normalize(u_sunDir.xyz);
	float day = smoothstep(-0.04, 0.16, L.z);
	float cloud = clamp(u_atmosphere.y, 0.0, 1.0);
	float clearSun = day * (1.0 - 0.45 * cloud);
	float shadowVis = day > 0.001 ? SampleSunShadow(v_worldPos, N) : 1.0;

	// Hemispherical blue-sky + asphalt-bounce illumination keeps shaded white doors clean and bright
	float up = N.z * 0.5 + 0.5;
	vec3 coolSky = mix(vec3(0.82, 0.90, 1.05), max(u_skyAmbient.rgb, vec3(0.42, 0.52, 0.70)) * 1.42, 0.46);
	vec3 streetBounce = vec3(0.82, 0.80, 0.78);
	float nightScale = mix(max(dot(u_ambLight.rgb, vec3(0.3333)) * 1.65, 0.18), 1.0, day);
	vec3 hemiAmb = mix(streetBounce, coolSky, up) * mix(0.70, 0.95, up) * nightScale;

	float shadStrength = clamp(max(u_sunMaterial.a, 0.65), 0.0, 1.0) * day * (1.0 - 0.40 * cloud);
	hemiAmb *= mix(vec3(0.72, 0.77, 0.86), vec3(1.0), mix(1.0, shadowVis, 0.68 * shadStrength));

	vec3 warmSun = mix(max(u_sunMaterial.rgb, vec3(0.60, 0.54, 0.46)), vec3(1.08, 0.98, 0.86), 0.55);
	float sunNdotL = max(dot(N, L), 0.0);
	float bodyWrap = clamp(dot(N, L) * 0.5 + 0.5, 0.0, 1.0);
	vec3 sunIrradiance = warmSun * clearSun * (0.64 * sunNdotL + 0.10 * bodyWrap) * shadowVis;
	vec3 extraDyn = v_lightFog.rgb * mix(0.55, 0.14, day);

	vec3 baseLin = pow(max(albedoSrgb, vec3(0.0)), vec3(2.2));
	vec3 diffuseHdr = baseLin * (hemiAmb + sunIrradiance + extraDyn);

	// Dual-paraboloid sky/horizon reflection in 512x512 EnvMapTex
	vec3 R = reflect(-V, N);
	vec3 Rp = normalize(vec3(R.xy, abs(R.z) + 1.0e-5));
	vec2 envUV = (Rp.xy / (1.0 + Rp.z)) * 0.485 + 0.5;
	vec3 envSrgb = texture(tex1, vec2(envUV.x, 1.0 - envUV.y)).rgb;
	vec3 envLin = pow(max(envSrgb, vec3(0.0)), vec3(2.2));
	float horizonFade = mix(0.60, 1.0, smoothstep(-0.55, 0.14, R.z));
	envLin *= horizonFade * mix(0.54, 1.0, shadowVis);

	// Dielectric clearcoat Fresnel (paint F0=0.045, glass F0=0.11, chrome F0=0.42)
	float ndotv = clamp(abs(dot(V, N)), 0.0, 1.0);
	float schlick = pow(1.0 - ndotv, 4.2);
	float isChrome = smoothstep(0.88, 0.94, u_reflProps.z) * isOpaque;
	float f0 = mix(0.045, 0.42, isChrome);
	f0 = mix(0.11, f0, isOpaque);
	float gloss = clamp(u_reflProps.z, 0.0, 1.0) * mix(0.03, 1.0, darkTrimMask) * smoothstep(0.0, 0.05, alpha);
	float reflWeight = clamp((f0 + (0.68 - f0) * schlick) * gloss, 0.0, 0.72);

	// Dual-lobe solar specular: razor-sharp clearcoat sun disk + warm metallic paint sheen
	float specMask = shadowVis * mix(0.04, 1.0, darkTrimMask) * clamp(u_reflProps.w, 0.15, 1.4);
	vec3 Hsun = normalize(V + L);
	float ndothSun = clamp(dot(N, Hsun), 0.0, 1.0);
	float sunFacing = smoothstep(-0.05, 0.18, dot(N, L));
	vec3 specHdr = warmSun * clearSun * sunFacing * specMask
	             * (pow(ndothSun, 210.0) * 1.55 + pow(ndothSun, 28.0) * 0.20 * baseLin + pow(ndothSun, 36.0) * 0.14);

	for(int i = 1; i < 5; i++){
		float dl = DoDirLightSpec(N, V, -u_specDir[i].xyz, 96.0);
		specHdr += dl * u_specColor[i].rgb * specMask * 0.35;
	}

	// Premultiplied-alpha composite (vehicleRenderCB uses SRCBLEND=ONE, DESTBLEND=INVSRCALPHA)
	vec3 opaqueHdr = diffuseHdr * (1.0 - reflWeight * 0.65) + envLin * reflWeight + specHdr;
	vec3 opaqueSrgb = ApplyFilmicToneAndGamma(opaqueHdr);
	vec3 glassReflSrgb = ApplyFilmicToneAndGamma(envLin * reflWeight + specHdr);
	vec3 glassBaseSrgb = ApplyFilmicToneAndGamma(diffuseHdr);

	vec4 color;
	if(isOpaque > 0.5){
		color.rgb = mix(u_fogColor.rgb, opaqueSrgb, visibility);
		color.a = 1.0;
	}else{
		color.rgb = mix(u_fogColor.rgb, glassBaseSrgb, visibility) * alpha * (1.0 - reflWeight)
		          + glassReflSrgb * visibility;
		color.a = clamp(alpha + reflWeight * (1.0 - alpha), 0.0, 1.0);
	}

	FRAGCOLOR(color);
}
