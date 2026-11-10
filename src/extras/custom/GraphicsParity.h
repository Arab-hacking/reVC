// GraphicsParity.h -- числовые константы и формулы графики клиента Black Russia,
// восстановленные из `libblackrussia-client.so` (PUNCT ANALYSE v38, пункт «Графика»).
//
// Модуль намеренно не зависит ни от librw, ни от игры: всё, что здесь есть,
// проверяется на хосте (tests/GraphicsParityTests.cpp) и используется реальным
// рендером (renderer/ParaboloidEnvMap.cpp, extras/postfx.cpp, шейдеры GL3).
//
// Каждая константа снабжена адресом из архива, чтобы расхождение было видно.
#pragma once

#include <cmath>
#include <cstdint>

namespace brgfx {

// ---------------------------------------------------------------------------
// Шейдерный ключ (глава 09 и 13.8)
// ---------------------------------------------------------------------------
// Шейдеры в клиенте не лежат готовыми: тело собирается в рантайме из фрагментов
// по 32-битной маске в w19 (`0x1732024` — вершинные, `0x1732fd0` — фрагментные).
// Доказанные биты:
//   бит 2  (0x4)      -> скиннинг + `uniform mat4 u_boneMatrices[32]` (`0x17326c0`)
//   бит 4  (0x10)     -> `uniform highp vec4 u_xform`                      (`0x17326c4`)
//   бит 6  (0x40)     -> блок отражения, объявляет `v_worldNormal`          (`0x1734af8`)
//   бит 20 (0x100000) -> второй бит того же набора объявлений нормали
//                        (маска `ands w26, w19, 0x100040` в `0x1732514`)
enum ShaderKeyBits
{
	KEY_SKINNING   = 2,
	KEY_XFORM      = 4,
	KEY_REFLECTION = 6,
	KEY_WORLDNAM20 = 20
};

// Маска, под которой объявляется `v_worldNormal` в обеих стадиях.
enum { KEY_WORLDNAMAL_MASK = 0x100040 };	// бит 6 | бит 20

inline uint32_t
KeyBit(int bit)
{
	return 1u << bit;
}

// Бит 6 включает сам блок отражения; бит 20 объявляет нормаль, но отражения
// без бита 6 не будет (в архиве блок подписан именно `tbz w19, #6`).
inline bool
KeyHasReflection(uint32_t key)
{
	return (key & KeyBit(KEY_REFLECTION)) != 0;
}

// Нормаль в шейдере объявлена при любом из двух бит набора.
inline bool
KeyHasWorldNormal(uint32_t key)
{
	return (key & KEY_WORLDNAMAL_MASK) != 0;
}

inline bool
KeyHasSkinning(uint32_t key)
{
	return (key & KeyBit(KEY_SKINNING)) != 0;
}

// ---------------------------------------------------------------------------
// Карта окружения (главы 03.3, 13.1, 13.2)
// ---------------------------------------------------------------------------
// Параболоидальная карта, а не кубическая: `waterclear256`, 256x256,
// общая на все материалы, обновляется не каждый кадр.
enum { ENVMAP_SIZE = 256 };

// `pow(envmap.rgb, vec3(0.8))`                                     (0x43f1e5)
constexpr float ENVMAP_POW = 0.8f;

// Ветка почти зеркальных поверхностей: `mix(color, reflectColour, 0.8)` (0x3e1220)
constexpr float MIRROR_MIX = 0.8f;

// `v_reflectTexCoords = clamp(v_reflectTexCoords, 0.001, 0.999);`   (0x3e11d5)
constexpr float REFLECT_UV_MIN = 0.001f;
constexpr float REFLECT_UV_MAX = 0.999f;

// Глубина в параболоидальном проходе: `length(ReflVector) * 0.002`  (0x42b527)
constexpr float PARABOLOID_DEPTH_SCALE = 0.002f;

// ---------------------------------------------------------------------------
// Солнечный блик (глава 13.5)
// ---------------------------------------------------------------------------
// `pow(max(dot(normalize(v_worldNormal), halfwayDir), 0.0), 50.0) * 0.4`
//                                                                  (0x40f29d)
constexpr float SUN_SPEC_POW = 50.0f;
constexpr float SUN_SPEC_SCALE = 0.4f;

// ---------------------------------------------------------------------------
// Формулы
// ---------------------------------------------------------------------------

inline void
ClampUV(float &u, float &v)
{
	u = u < REFLECT_UV_MIN ? REFLECT_UV_MIN : (u > REFLECT_UV_MAX ? REFLECT_UV_MAX : u);
	v = v < REFLECT_UV_MIN ? REFLECT_UV_MIN : (v > REFLECT_UV_MAX ? REFLECT_UV_MAX : v);
}

// `ReflPos = normalize(Out_Refl.xy) * (Out_Refl.z * 0.5 + 0.5)`      (0x434bb8)
// `gl_Position = vec4(ReflPos.xy, length(ReflVector) * 0.002, 1.0)` (0x42b527)
// Результат лежит в единичном круге [-1,1] — это полярная развёртка карты,
// а НЕ UV в [0,1]. Клиент здесь ничего не зажимает: кламп 0.001..0.999
// (0x3e11d5) стоит в ДРУГОЙ ветке — экранных отражениях `tex8`, и к
// параболоидальной карте отношения не имеет. Не путать.
inline void
ParaboloidUV(float rx, float ry, float rz, float &u, float &v)
{
	float len2 = rx * rx + ry * ry;
	float scale = rz * 0.5f + 0.5f;
	if(len2 > 1e-12f){
		float inv = 1.0f / std::sqrt(len2);
		u = rx * inv * scale;
		v = ry * inv * scale;
	}else{
		u = 0.0f;
		v = 0.0f;
	}
}

// Перевод полярной развёртки в UV текстурного сэмпла: карта 256x256 хранит
// параболоид целиком, поэтому [-1,1] -> [0,1].
inline void
ParaboloidToUV(float u, float v, float &su, float &sv)
{
	su = u * 0.5f + 0.5f;
	sv = v * 0.5f + 0.5f;
}

// Глубина параболоидального прохода: длина вектора отражения с масштабом 0.002.
inline float
ParaboloidDepth(float rx, float ry, float rz)
{
	return std::sqrt(rx * rx + ry * ry + rz * rz) * PARABOLOID_DEPTH_SCALE;
}

// `fresnelFactor = 1.0 - abs(dot(-viewVec, v_worldNormal))`            (0x43f19d)
// Внимание: у клиента в этой ветке НЕТ ни степени, ни клампа — это «сырая»
// френель. Степень 3.5 и кламп 0.15..0.70 стоят в другом (мобильном) теле.
inline float
Fresnel(float viewX, float viewY, float viewZ,
        float normalX, float normalY, float normalZ)
{
	float d = -(viewX * normalX + viewY * normalY + viewZ * normalZ);
	if(d < 0.0f)
		d = -d;
	float f = 1.0f - d;
	return f < 0.0f ? 0.0f : f;
}

// Цепочка клиента, дословно (глава 13.1):
//   envmap.rgb = texture(tex2, ReflPos).rgb;            (0x3e1241)
//   envmap.rgb = envmap.rgb * fresnelFactor * u_envMapCoef;  (0x40b632)
//   envmap.rgb = pow(envmap.rgb, vec3(0.8));            (0x43f1e5)
inline float
EnvMapTerm(float texel, float fresnel, float coef)
{
	float m = texel * fresnel * coef;
	if(m < 0.0f)
		m = 0.0f;
	return std::pow(m, ENVMAP_POW);
}

// `color.rgb += (diffuseVec + specular + envmap.rgb);`                 (0x4309d9)
inline float
ReflectedColor(float diffuse, float specular, float envmapTerm)
{
	return diffuse + specular + envmapTerm;
}

// Применение почти зеркальной ветки: `mix(color, reflectColour, 0.8)`.
inline float
MirrorMix(float base, float reflectColour)
{
	return base + (reflectColour - base) * MIRROR_MIX;
}

// ---------------------------------------------------------------------------
// Туман (глава 12)
// ---------------------------------------------------------------------------
// `v_fog = clamp((length(Vertex.xyz - u_eye.xyz) - u_fogData.x) * u_fogData.y, 0, 1);`
// `color.rgb = mix(color.rgb, u_fogColor.rgb, v_fog);`
// u_fogData.x = FogStart, u_fogData.y = 1/(FarClip - FogStart).
inline float
FogFactor(float distance, float fogStart, float invRange)
{
	float t = (distance - fogStart) * invRange;
	if(t < 0.0f)
		return 0.0f;
	return t > 1.0f ? 1.0f : t;
}

inline float
FogMix(float color, float fogColor, float fogFactor)
{
	return color + (fogColor - color) * fogFactor;
}

// librw хранит `fogRange = 1/(fogStart - fogEnd)`, а клиент — `1/(FarClip-FogStart)`.
// Знак разный, поэтому обратная величина берётся от модуля разности.
inline float
FogInvRange(float fogStart, float fogEnd)
{
	float span = fogEnd - fogStart;
	if(span < 0.0f)
		span = -span;
	if(span < 1e-4f)
		span = 1e-4f;
	return 1.0f / span;
}

// ---------------------------------------------------------------------------
// Постобработка (глава 09.2)
// ---------------------------------------------------------------------------
// Четыре отдельные прохода по полноэкранному кваду: размытие -> контраст ->
// виньетка -> контур. Тела восстановлены целиком; ниже — их константы.

// blur_0x404248: `const int mSize = 5; float sigma = 7.0;`
enum { BLUR_KERNEL_SIZE = 5 };
constexpr float BLUR_SIGMA = 7.0f;

// `float normpdf(float x, float sigma)`
inline float
BlurNormPdf(float x, float sigma)
{
	return 0.39894f * std::exp(-0.5f * x * x / (sigma * sigma)) / sigma;
}

// Ядро размытия и его нормализующий множитель Z = sum(kernel).
// Тело клиента: kernel[kSize+j] = kernel[kSize-j] = normpdf(j, sigma),
// затем final = sum(kernel[j]*kernel[i]*texel) / (Z*Z).
inline void
BlurKernel(float *kernel, float &Z)
{
	const int kSize = (BLUR_KERNEL_SIZE - 1) / 2;
	for(int j = 0; j <= kSize; j++)
		kernel[kSize + j] = kernel[kSize - j] = BlurNormPdf((float)j, BLUR_SIGMA);
	Z = 0.0f;
	for(int j = 0; j < BLUR_KERNEL_SIZE; j++)
		Z += kernel[j];
}

// vignette_0x3fd3ac:
//   radius = sqrt(pow2(uv.x-0.5)*params.x + pow2(uv.y-0.5))
//   fade   = clamp(radius*params.y + params.z, 0, 1)
//   rgb    = mix(rgb*contrastMult + contrastAdd, vignetteColor.rgb, fade*vignetteColor.a)
inline float
VignetteFade(float u, float v, float paramsX, float paramsY, float paramsZ)
{
	float du = u - 0.5f;
	float dv = v - 0.5f;
	float radius = std::sqrt(du * du * paramsX + dv * dv);
	float fade = radius * paramsY + paramsZ;
	if(fade < 0.0f)
		return 0.0f;
	return fade > 1.0f ? 1.0f : fade;
}

// ВАЖНО о значениях. `0x171b704` (загрузчик постобработки) загружает три
// vec3/vec4 параметра виньетки из `.rodata` (адреса вида `0x3d2000+0x5e0`),
// а самих байтов `.rodata` в архиве нет — доказана только ФОРМУЛА.
// Поэтому численные параметры виньетки приходят из конфигурации клиента
// (reVC.ini, [Rendering] Vignette*), а не подставляются «похожими» числами.

// outline_0x43baf8: контур поверх сцены
//   rast.rgb = outline.rgb*outline.a + rast.rgb*(1-outline.a)
//   rast.rgb = rast.rgb*contrastMult + contrastAdd
inline float
OutlineBlend(float base, float outline, float alpha)
{
	return outline * alpha + base * (1.0f - alpha);
}

// ---------------------------------------------------------------------------
// Материал (глава 13.3): полный PBR-набор полей `librw/matfx.cpp` (0x1750a38)
// ---------------------------------------------------------------------------
// Отражение на конкретной модели регулируется metalnessTex/roughnessTex/
// reflectanceTex. Перечисление — чтобы порядок полей совпадал с архивом.
enum MaterialMap
{
	MATMAP_ALBEDO,
	MATMAP_DIFFUSE,
	MATMAP_NORMALS,
	MATMAP_ROUGHNESS,
	MATMAP_METALNESS,
	MATMAP_AO,
	MATMAP_REFLECTANCE,
	MATMAP_HEIGHTS,
	MATMAP_TRANSLUCENCY,
	MATMAP_EMISSION,
	MATMAP_LIGHTMAP,
	MATMAP_INTERIORS,
	MATMAP_LANDMASK,
	MATMAP_COUNT
};

} // namespace brgfx
