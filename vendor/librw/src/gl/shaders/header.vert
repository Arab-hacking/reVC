
//#define DIRECTIONALS
//#define POINTLIGHTS
//#define SPOTLIGHTS

#define ATTRIB_POS	0
#define ATTRIB_NORMAL	1
#define ATTRIB_COLOR	2
#define ATTRIB_WEIGHTS	3
#define ATTRIB_INDICES	4
#define ATTRIB_TEXCOORDS0	5
#define ATTRIB_TEXCOORDS1	6


VSIN(ATTRIB_NORMAL)	vec3 in_normal;
VSIN(ATTRIB_COLOR)	vec4 in_color;
VSIN(ATTRIB_WEIGHTS)	vec4 in_weights;
VSIN(ATTRIB_INDICES)	vec4 in_indices;
VSIN(ATTRIB_TEXCOORDS0)	vec2 in_tex0;
VSIN(ATTRIB_TEXCOORDS1)	vec2 in_tex1;


#ifdef USE_UBOS
layout(std140) uniform State
{
	vec2 u_alphaRef;
	vec4  u_fogData;
	vec4  u_fogColor;
};
#else
uniform vec4 u_alphaRef;
uniform vec4  u_fogData;
uniform vec4  u_fogColor;
#endif

#define u_fogStart (u_fogData.x)
#define u_fogEnd (u_fogData.y)
#define u_fogRange (u_fogData.z)
#define u_fogDisable (u_fogData.w)

// --- Black Russia client material parity ---
uniform vec4 u_eye;		// xyz: camera world position
uniform vec4 u_sunDir;	// xyz: direction TO the sun
uniform vec4 u_atmosphere;	// x: enabled, y: clouds, z: fog, w: dayNightBalance
uniform mat4 u_uvMatrix;


#ifdef USE_UBOS
layout(std140) uniform Scene
{
	mat4 u_proj;
	mat4 u_view;
};
#else
uniform mat4 u_proj;
uniform mat4 u_view;
#endif

#define MAX_LIGHTS 8

#ifdef USE_UBOS
layout(std140) uniform Object
{
	mat4  u_world;
	vec4  u_ambLight;
	vec4 u_lightParams[MAX_LIGHTS];	// type, radius, minusCosAngle, hardSpot
	vec4 u_lightPosition[MAX_LIGHTS];
	vec4 u_lightDirection[MAX_LIGHTS];
	vec4 u_lightColor[MAX_LIGHTS];
};
#else
uniform mat4 u_world;
uniform vec4 u_ambLight;
uniform vec4 u_lightParams[MAX_LIGHTS];	// type, radius, minusCosAngle, hardSpot
uniform vec4 u_lightPosition[MAX_LIGHTS];
uniform vec4 u_lightDirection[MAX_LIGHTS];
uniform vec4 u_lightColor[MAX_LIGHTS];
#endif

uniform vec4 u_matColor;
uniform vec4 u_surfProps;	// amb, spec, diff, extra

#define surfAmbient (u_surfProps.x)
#define surfSpecular (u_surfProps.y)
#define surfDiffuse (u_surfProps.z)

vec3 DoDynamicLight(vec3 V, vec3 N)
{
	vec3 color = vec3(0.0, 0.0, 0.0);
	for(int i = 0; i < MAX_LIGHTS; i++){
		if(u_lightParams[i].x == 0.0)
			break;
#ifdef DIRECTIONALS
		if(u_lightParams[i].x == 1.0){
			// direct
			float l = max(0.0, dot(N, -u_lightDirection[i].xyz));
			color += l*u_lightColor[i].rgb;
		}else
#endif
#ifdef POINTLIGHTS
		if(u_lightParams[i].x == 2.0){
			// point
			vec3 dir = V - u_lightPosition[i].xyz;
			float dist = length(dir);
			float atten = max(0.0, (1.0 - dist/u_lightParams[i].y));
			float l = max(0.0, dot(N, -normalize(dir)));
			color += l*u_lightColor[i].rgb*atten;
		}else
#endif
#ifdef SPOTLIGHTS
		if(u_lightParams[i].x == 3.0){
			// spot
			vec3 dir = V - u_lightPosition[i].xyz;
			float dist = length(dir);
			float atten = max(0.0, (1.0 - dist/u_lightParams[i].y));
			dir /= dist;
			float l = max(0.0, dot(N, -dir));
			float pcos = dot(dir, u_lightDirection[i].xyz);	// cos to point
			float ccos = -u_lightParams[i].z;
			float falloff = (pcos-ccos)/(1.0-ccos);
			if(falloff < 0.0)	// outside of cone
				l = 0.0;
			l *= max(falloff, u_lightParams[i].w);
			return l*u_lightColor[i].rgb*atten;
		}else
#endif
			;
	}
	return color;
}

vec3 DoPointSpotLight(vec3 V, vec3 N)
{
	vec3 color = vec3(0.0, 0.0, 0.0);
	float nLen2 = dot(N, N);
	vec3 nrm = nLen2 > 1.0e-8 ? N * inversesqrt(nLen2) : vec3(0.0, 0.0, 1.0);
	for(int i = 0; i < MAX_LIGHTS; i++){
		if(u_lightParams[i].x == 0.0)
			break;
#ifdef POINTLIGHTS
		if(u_lightParams[i].x == 2.0){
			vec3 dir = V - u_lightPosition[i].xyz;
			float dist = length(dir);
			float atten = max(0.0, (1.0 - dist / max(u_lightParams[i].y, 0.001)));
			float l = dist > 1.0e-5 ? max(0.0, dot(nrm, -dir / dist)) : 1.0;
			color += l * u_lightColor[i].rgb * atten;
		}else
#endif
#ifdef SPOTLIGHTS
		if(u_lightParams[i].x == 3.0){
			vec3 dir = V - u_lightPosition[i].xyz;
			float dist = length(dir);
			float atten = max(0.0, (1.0 - dist / max(u_lightParams[i].y, 0.001)));
			vec3 ndir = dist > 1.0e-5 ? (dir / dist) : vec3(0.0, 0.0, -1.0);
			float l = max(0.0, dot(nrm, -ndir));
			float pcos = dot(ndir, u_lightDirection[i].xyz);
			float ccos = -u_lightParams[i].z;
			float falloff = (pcos - ccos) / max(1.0 - ccos, 0.001);
			if(falloff < 0.0)
				l = 0.0;
			l *= max(falloff, u_lightParams[i].w);
			color += l * u_lightColor[i].rgb * atten;
		}else
#endif
			;
	}
	return color;
}

// Building.vs + GL3Shader.vs (Shaders For BLAYZ RUSSIA PC):
// - Preserves subtle daytime vertex AO/tint from geo->colors without legacy darkening;
// - Applies the 0.25 night ambient lift from Building.vs when Geometry::PRELIT is present;
// - Keeps full 1.0 base colour on non-prelit geometries (GL3Shader.vs).
vec4 BR_EvalBuildingVertexColor(vec4 inColor, vec4 matColor, vec3 worldPos, vec3 worldNormal)
{
	float isUnlit = step(1.5, u_surfProps.w);
	float hasPrelit = step(0.5, u_surfProps.w - 2.0 * isUnlit);
	float dayNight = clamp(u_atmosphere.w, 0.0, 1.0);
	vec3 dayVert = mix(vec3(1.0), clamp(inColor.rgb * 1.12 + vec3(0.14), 0.0, 1.0), 0.60);
	vec3 nightVert = mix(vec3(1.0), clamp(inColor.rgb * 1.05 + vec3(0.28), 0.0, 1.25), 0.55);
	vec3 prelitCol = mix(dayVert, nightVert, dayNight);
	vec3 litCol = mix(vec3(1.0), prelitCol, hasPrelit) + DoPointSpotLight(worldPos, worldNormal) * surfDiffuse;
	vec3 unlitCol = mix(vec3(1.0), inColor.rgb, hasPrelit);
	vec3 col = mix(litCol, unlitCol, isUnlit);
	float outAlpha = mix(1.0, inColor.a, hasPrelit) * matColor.a;
	return vec4(col * matColor.rgb, outAlpha);
}

// Skin.vs / GL3Shader.vs (Shaders For BLAYZ RUSSIA PC): pass material colour +
// local point/spot lights without baking legacy vertex ambient.
vec4 BR_EvalDynamicVertexColor(vec4 inColor, vec4 matColor, vec3 worldPos, vec3 worldNormal)
{
	vec3 col = vec3(1.0) + DoPointSpotLight(worldPos, worldNormal) * surfDiffuse;
	return vec4(col * matColor.rgb, inColor.a * matColor.a);
}

float DoFog(float w)
{
	return clamp((w - u_fogEnd)*u_fogRange, u_fogDisable, 1.0);
}

// Distance fog measured from the eye (as in the Black Russia client);
// same output polarity as DoFog: 1 = clear, 0 = fully fogged.
float DoFogEye(vec3 worldPos)
{
	if(u_fogDisable > 0.5)
		return 1.0;
	float d = length(worldPos - u_eye.xyz);
	float t = (d - u_fogStart) / max(u_fogEnd - u_fogStart, 0.0001);
	return 1.0 - clamp(t, 0.0, 1.0);
}
