# ГРАФИКА — Black Russia `libblackrussia-client.so`

Реализация графического пункта из `PUNCT-ANALYSE-v38`. Всё ниже — из архива
(дизассемблер `Код из библиотеки/`, главы `Анализ/09-shader-system.md`,
`12-туман.md`, `13-отражения.md`), без «похожих» значений.

Проверки: `tests/GraphicsParityTests.py`-эквивалент `tests/GraphicsParityTests.cpp`
(**100 проверок**), `tests/GL3ShaderTests.py` (**10 тестов**, включая два новых
контрактных), `node prog.js` (сборка всех программ GLSL, `fails=0`).

---

## 1. Карта окружения на параболоиде (глава 13)

Клиент рисует отражения не в кубическую карту, а в **параболоидальную**
256×256 (`waterclear256`, одна на сцену, `05-textures.md`).

Вершинная проекция при захвате (`0x42b527`), буквально:

```glsl
vec3 ReflVector = worldPos - u_eye.xyz;
vec3 ReflPos = normalize(ReflVector);
vec2 planar = ReflPos.xy;
float plen2 = dot(planar, planar);
planar = plen2 > 1e-12 ? planar * inversesqrt(plen2) : vec2(0.0);
ReflPos.xy = planar * (ReflPos.z * 0.5 + 0.5);
gl_Position = vec4(ReflPos.xy, length(ReflVector) * 0.002, 1.0);
```

Важно: **UV не зажимается** в `0.001..0.999`. Этот clamp (`0x3e11d5`) стоит в
другой, экранной ветви `tex8`; приписывать его параболоиду — ошибка. Поэтому
`GraphicsParity.h::ParaboloidUV` возвращает отрицательные значения как есть, а
`ParaboloidToUV` делает единственное, что нужно на выборке: `[-1,1] → [0,1]`.

Выборка в материале (`0x434bb8` + `0x40b632` + `0x43f19d` + `0x43f1e5`):

```glsl
vec3 refl = reflect(-V, N);
fres = 1.0 - abs(dot(-V, N));
env  = texture(envMap, ParaboloidToUV(...)) * fres * u_envMapCoef;
env  = pow(env, 0.8);
color.rgb += diffuse + specular + env;
```

### Что изменено в reVC

| Файл | Изменение |
| --- | --- |
| `src/renderer/ParaboloidEnvMap.{h,cpp}` | Захват: `rwRASTERTYPECAMERATEXTURE` 256×256, z-буфер, `RwTexture`, `UpdateStride()`, `Capture(renderScene, stride)` |
| `vendor/librw/src/gl/shaders/header.vert` | `ParaboloidProjection()` — арифметика `0x42b527` дословно, NaN-безопасно |
| `default.vert`, `skin.vert` | `gl_Position = u_paraboloid.x > 0.5 ? ParaboloidProjection(Vertex.xyz) : u_proj*u_view*Vertex;` |
| `simple.frag` | плоская ветвь цвета на время захвата |
| `header.frag` | `uniform vec4 u_paraboloid;`, `uniform vec4 u_envMapMode;` |
| `matfx_env.frag` | полярная выборка карты при `u_envMapMode.x > 0.5`; иначе прежний `u_texMatrix` |
| `gl3device.cpp` | `u_paraboloid`, `u_envMapMode`, `begin/endParaboloidPass`, `set/getEnvMapMode`, `setCaptureEyePosition` |
| `gl3matfx.cpp` | `setMatFXParaboloidTex()` — переключение выборки только для текстуры захвата |
| `gl3pipe.cpp` | ветка захвата в `ObjPipeline::render` |
| `VehicleModelInfo.cpp` | `RefreshReflectionEnvTexture()` — материалы машин переводятся на живую карту |

Ключевое решение: проходу захвата **не нужна камера**. `ParaboloidProjection`
считает NDC прямо из позиции вершины, поэтому `RwCameraBeginUpdate` (который
перезаписал бы `u_eye`, `u_proj`, `u_view` и viewport главной камеры) не
вызывается вовсе. Вместо этого — `beginParaboloidPass(fbo, w, h, skyColor)` /
`endParaboloidPass()`; выход восстанавливает `u_eye` из сохранённой view-матрицы.

Материалы, у которых env-map — **не** наша текстура (родные TXD-карты игры),
продолжают идти по `u_texMatrix`, как раньше: поведение не ломается.

## 2. Туман (глава 12) — уже соответствовал

```
VS: v_fog = clamp((length(V - u_eye.xyz) - u_fogData.x) * u_fogData.y, 0, 1)
FS: rgb = mix(rgb, u_fogColor.rgb, v_fog)
```
`u_fogData.y = 1 / (FarClip - FogStart)` — совпадает с `DoFogEye` в
`header.vert`. Правок не потребовалось.

## 3. Пост-обработка (глава 11)

Тела переносимы и лежат в `GraphicsParity.h`:

* виньетка (`0x3fd3ac`) — `radius = sqrt((uv.x-0.5)^2*params.x + (uv.y-0.5)^2)`,
  `fade = clamp(radius*params.y + params.z, 0, 1)`, затем `mix(rgb*contrastMult +
  contrastAdd, vignetteColor.rgb, fade*vignetteColor.a)`;
* размытие (`0x404248`) — 5 отсчётов, `normpdf = 0.39894*exp(-0.5j^2/sigma^2)/sigma`,
  `sigma = 7`, деление на `Z^2`;
* контур (`0x43baf8`) — `mix(scene, outline.rgb, outline.a)`, затем контраст.

**Числовые параметры виньетки/контраста в архиве отсутствуют**: они лежат в
`.rodata` (`0x171b704` ← `0x3d2000 + 0x5e0`) и в библиотеку не попали. Поэтому
`.frag`-файлы `brVignette/brBlur/brOutline` **не добавлены**: их uniform-блоки
отсутствуют в архиве, и файлы были бы мёртвым кодом с выдуманными числами.
Формулы зафиксированы в `GraphicsParity.h` и покрыты тестом — как только
появятся реальные значения, шейдер собирается без домыслов.

## 4. Что осталось за рамками

* Инстансинг биллбордов (40 в клиенте) и PBR-набор из 13 карт — API librw нет,
  вводить его значило бы переписать пайплайн, а не воспроизвести значения.
* ETC2-загрузка: в librw есть S3TC и ASTC, ETC2-пути в клиенте не найдено.
* Noesis/VR-шейдеры (`01_bodies/noesis_*`, `vr_*`) — не часть игры.
