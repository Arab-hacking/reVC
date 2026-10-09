# Паритет рендера с клиентом Black Russia (порт из `libblackrussia-client.so`)

Анализ библиотеки клиента: **[BR_LIB_ANALYSIS.md](BR_LIB_ANALYSIS.md)** — как клиент
нативно читает `.mod`/`.btx`, SA-скины/модели и из чего собран их рендер
(реконструкции их world-шейдеров: `brso/shaders_reconstructed/`,
полный дамп кусков: `brso/world_shader_pieces.txt`).

Что клиент делает иначе базового librw (и что теперь делает reVC):

| Элемент | Было (librw) | Стало (как в клиенте) |
|---|---|---|
| Солнечный свет | только vertex-лайтинг (ambient + динамические источники) | **попиксельный**: `diffuse = N·sun`, Блинн-Фонг `pow(N·H, 50)·0.4` с силой `(0.2,0.2,0.2)` |
| Тень с обратной стороны | нет | `shadow = 0.3·(1−v_diffuse)^24`, затем `color·=(1−shadow)` |
| Формула тумана | по `gl_Position.w` | **дистанция от глаза**: `clamp((length(pos−eye)−start)/(end−start))` |
| Envmap (матFX, машины) | сферические координаты по нормали × свет | **fresnel**: `reflect(view, normal)` → `pow(env, 0.8)·clamp(fresnel³·⁵, 0.15, 0.7)·коэф` |
| UV-трансформ | нет | `v_tex0 = u_uvMatrix · uv` (по умолчанию единичная) |
| Гамма | нет | `pow(color, u_localInvGamma)` после тумана |
| Позиция камеры | не передавалась в шейдер | `u_eye` вычисляется из view-матрицы |

## API (librw)

```cpp
rw::setSunDirection(toSun);   // направление К солнцу; зовётся каждый кадр
                              // из CRenderer::ConstructRenderList (CTimeCycle)
rw::setLocalInvGamma(g);     // по умолчанию 1.0 (без изменения картинки)
```

Туман берётся из обычного `rwRENDERSTATEFOGSTART/END` (формула расстояния от глаза);
отключение тумана (`FOGENABLE = false`) сохраняет поведение.

## Где правки

* `vendor/librw/src/gl/shaders/`: `header.vert`, `header.frag`, `default.vert`,
  `skin.vert`, `matfx_env.vert`, `matfx_env.frag`, `simple.frag` (+ пересборка `.inc`
  через `make` в этой папке — только sed, без внешних утилит).
* `vendor/librw/src/gl/gl3device.cpp`: новые uniform'ы (`u_eye`, `u_sunDir`,
  `u_localInvGamma`, `u_uvMatrix`), выгрузка в `setViewMatrix`.
* `vendor/librw/src/gl/gl3skin.cpp`: `#define BRMATERIAL` для FS скинов.
* `vendor/librw/src/rwengine.h` + `src/engine.cpp`: глобальное состояние солнца/гаммы.
* `src/renderer/Renderer.cpp`: `rw::setSunDirection(CTimeCycle::GetSunDirection())`.
* FS-блок под `#define BRMATERIAL` — только у world-программ (default/skin);
  im2d/im3d работают по-старому (там свои VS без новых varying'ов).

## Проверки

* Все пары шейдеров (default/fullLight/noAT, skin, matfx, im2d, im3d; GL3 330 и
  ES 310) прогнаны через `glslangValidator -l` — 0 ошибок.
* Полная сборка Linux 318/318, mingw-TU (gl3device, gl3skin, engine, Renderer) — OK.
* Мировые эффекты клиента (shadow-map с poisson, вода, снег, терраин-слои, небо,
  PBR-emission, пост-градация) остаются за портом на данных BR — в отчёте §4.

---

## Дополнение: освещение и небо клиента из `data/timecyc.json` (этап 2)

Модель поверхности (солнце+спекуляр+тень+гамма+envmap fresnel) — см. таблицу выше; она
работает в паре с **таймциклом клиента**: если в `custom` лежит `data/timecyc.json`
(из common-архива BR), `CCustomTimecycle::Apply()` (src/extras/custom/CustomTimecycle.cpp)
заполняет таблицы `CTimeCycle` его значениями после `CTimeCycle::Initialise()`. Небо
(`SkyTop/SkyBottom`), солнце (`SunCore/SunCorona/SunSize`), туман и FarClip, ambient
(включая «физический» AmbientPhysical → объектный ambient), вода — по клиенту.
Проверено стендом `tools/custom-folder-verification/test_custom2` на настоящих данных
клиента: слоты, интерполяция часов, отсутствие файлов = игра остаётся на своём цикле.
`PostFX1/2` клиента в этой игре аналогов не имеет и пропускается.
