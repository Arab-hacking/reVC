#ifndef __GTA_PARABOLOIDENVMAP_H__
#define __GTA_PARABOLOIDENVMAP_H__

// Параболоидальная карта окружения клиента Black Russia
// (renderer/ParaboloidEnvMap.cpp в архиве, 45 функций / 4 160 инструкций).
//
// Механизм восстановлен по строкам шейдера:
//   * карта одна на кадр, 256x256, «waterclear256» (глава 03.3);
//   * проход пишется параболоидальной проекцией, а не перспективной камерой;
//   * при сэмплировании: texel * fresnel * u_envMapCoef, затем pow(.., 0.8).
//
// Проекция прохода (0x42b527), применяется к КАЖДОЙ вершине сцены:
//   vec3 ReflVector = Vertex.xyz - u_eye.xyz;
//   vec3 ReflPos = normalize(ReflVector);
//   ReflPos.xy = normalize(ReflPos.xy) * (ReflPos.z * 0.5 + 0.5);
//   gl_Position = vec4(ReflPos.xy, length(ReflVector) * 0.002, 1.0);
//
// Точка захвата ставится в камеру игрока (u_eye = позиция камеры), поэтому
// выборка по направлению отражения даёт ту же карту, что и у клиента.
class CParaboloidEnvMap
{
public:
	enum { MAP_SIZE = 256 };

	static bool Init(void);
	static void Shutdown(void);
	static bool IsReady(void);

	// Пересобрать карту. stride пропускает кадры, если на устройстве включён
	// дешёвый режим (карта обновляется не каждый кадр, глава 13.2).
	// renderScene обходит атомы мира обычным путём; внутри прохода пайплайн
	// сам переключается на плоский захват.
	static bool Capture(void (*renderScene)(void), int32 stride);
	static void Invalidate(void);

	// Шаг обновления карты в кадрах. Клиент обновляет её не каждый кадр
	// (глава 13.2), поэтому шаг зависит от уровня графики телефона.
	static int32 UpdateStride(void);

	// Текстура карты (rw::Texture*) или nil, пока карта не готова.
	static void *GetTexture(void);

	// Отладочный вывод последней ошибки инициализации (nil, если всё хорошо).
	static const char *GetError(void);
};

#endif // __GTA_PARABOLOIDENVMAP_H__
