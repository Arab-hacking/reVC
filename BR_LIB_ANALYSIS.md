# Анализ `libblackrussia-client.so` (Android ARM64, 27 МБ)

Клиент **Black Russia Online** (`com.blackhub.bronline`) — ELF aarch64, stripped, NDK r28c.
Полезная нагрузка: свой движок на базе GTA VC (та же линейка, что и reVC: строки
`CdStream`, `FinishLoadClumpFile`, `LoadPedObject`, `Shadows.cpp` с `StoreShadowForPed`)
+ **свой форк librw** (`BRClient/src/librw/...`, CI на GitLab) + GLES3/EGL, BASS, ENet,
OpenSSL, mpg123/opus, miniz, TinyKtx, nlohmann::json, eastl.

Все адреса ниже — в_offsets файла (file offset == VA для RX-сегмента).

## 1. Как читают `.mod` — ПОЛНЫЙ ПАРИТЕТ с нашей реализацией

Функция расшифровки: **0xc34dd8** (единственный вызов — модельный пайплайн, `BL 0xc148a0`).

```
if (*(u32*)buf != 0xAB921033) return;         // магия
nBlocks = *(u32*)(buf + 8);                    // счётчик блоков
for (i = 0; i < nBlocks; i++)
    tea_decrypt8(buf + 0x1C + i*0x800, 0x800, key, rounds=8);   // TEA, 8 раундов
memmove(buf, buf + 0x1C, *(u32*)(buf + 4));    // компактизация: длина @ +4
```

* **Ключ** лежит в .rodata по `0x314a9c` (16 байт), маскируется так:
  `key[i] = ror32(raw[i] ^ 0x12913AFB, 19)`.
  `raw = {0x6ED9EE7A, 0x930C666B, 0x930E166B, 0x4709EE79}` — **байт-в-байт совпадает с
  `MOD_KEY_RAW`/`MOD_KEY_XOR` в нашем `br/brformats.h`**, ror19 = наш `modKey(variant 0)`.
* TEA-цикл (0xc34ee8): `sum = 0x9E3779B9 * rounds`, декод `v1 -= f(v0,k2,k3); v0 -= f(v1,k0,k1)`,
  `sum -= 0x61C88647` на круге — классический TEA-decrypt, 8 кругов = наш `tea_decrypt`.
* После расшифровки буфер уходит в `RwStreamOpen(type=3=memory, read)` (0xc148e0) и
  таблицу диспетчеризации по типу (`0x1900750`) → clump-загрузчик.
* Нормализация RW-потока (версии/флаги) в этом .so не нужна — их ассеты уже в
  поддерживаемом формате; у нас `understandModInplace` дополнительно чинит заголовки.

**Вывод:** формат, ключ, режим, компоновка заголовка идентичны `decryptModInplace`/`understandModInplace` — читаем так же нативно, как клиент.

## 2. Как читают `.btx` — тоже паритет (u32 flags + KTX11)

* Парсер — **TinyKtx** (строки `"TinyKtx must have * callback"`, `"Not a KTX file or
  corrupted as identified isn't valid"`, `"KTX 11"`).
* Точка входа `0x12f4094` (и `0x12f4940`): `RwStreamOpen(3,1,{ptr,size})` → `0x12f3924` —
  разбор KTX: сравнение 12-байтового идентификатора `AB 4B 54 58 20 31 31 BB 0D 0A 1A 0A`,
  `endianness == 0x04030201` (иначе байт-свап или ошибка).
* **Вызывающая сторона (0x12f6d5c): `add x1, x22, #4`** — буфер файла передаётся **с
  поправкой +4**, а первый u32 (`ldr w8,[x22]` @0x12f6c84) сохраняется в поле текстуры
  (флаги: `0x08` = сжатая, `0x01` = есть альфа — см. наш `brtex.h`).
* Формат файла `.btx` = **`u32 flags` + KTX11 (ASTC/DXT/uncompressed)** — ровно наше
  `isBtx()` (`memcmp(p+4, KTX1)`).

**Вывод:** `parseBtx` эквивалентен их пути (flags + TinyKtx), включая ASTC.

## 3. SA-скины и модели

* **Скины:** в форке есть `librw/skin.cpp` (assert на строке 52) и **`gl3skin.cpp`**
  (GPU-скиннинг, `u_boneMatrices[32]`, вариант с инстансингом `u_worldInstancing[40]`) —
  тот же стек, что у нас (`skin.cpp` + `gl3skin.cpp` + `skin.vert`, у нас лимит 64 кости).
* **Анимации:** найден **`ANPK`** (корень SA/VC-словарей) и **`ReadANPK`**; `ANP3`/`ANP2`
  в бинаре **нет** — то есть они читают только `.ifp` с корнем `ANPK` (большой SA
  `PED.IFP` как раз имеет корень ANPK). Имена анимаций в .so — SA-набор
  (`Tablet_Idle3`, `chainsaw`, `player2armed`, `Grlfrd_Kiss_01`, …).
  Наш движок читает ANPK штатным путём (SA-совместимый читальщик) **плюс** умеет
  `ANP3/ANP2` сверх их функционала.
* **Модели:** после расшифровки `.mod` — диспетчер по типу (таблица `0x1900750`) →
  `RwStream(memory)` → clump; `MITYPE_VEHICLE/PED/CLUMP`-маршрут как в VC.
  Skin-чанк проходит через `readSkin` librw (та же раскладка `numBones,numUsedBones,
  indices,weights,0xdeaddead,inverseMatrices`, что в нашей `vendor/librw/src/skin.cpp`).

## 4. Рендер (GLSL)

Их форк librw **новее нашего** (есть `gl3renderqueue.cpp`, у нас его нет) и собран на
том же фреймворке шейдеров (`VSIN/VSOUT/FSIN/FRAGCOLOR`, `#define ATTRIB_*`). Шейдеры
комбинируются в рантайме из кусков-строк через `strcat` (сборщик **VS — 0x113b3dc+**,
**FS — 0x113c3b0+**). Полный дамп кусков: `world_shader_pieces.txt` (1082 строки).

### Мировой материал (суть отличий от нашего librw)

**Vertex shader:**
* `v_tex0 = (u_uvMatrix * vec4(in_tex0,0,1)).xy` — UV-матрица (у нас нет).
* `diffuse = max(dot(N, -u_sunDir), 0)` — попиксельный свет считается во FS,
  во VS только этот коэффициент.
* `v_color.rgb += u_materialAmbient.rgb; v_color.a *= u_materialAmbient.a; clamp`.
* Туман **от глаза**: `v_fog = clamp((length(Vertex - u_eye) - fogStart) * fogScale, 0, 1)`
  (`u_fogData` = vec2) — у нас туман по `gl_Position.w` (дистанция камеры, другой путь).
* Скиннинг (4 кости, как у нас), инстансинг (40), outline-экспансия, clip plane,
  shadow-proj (`u_projLight*u_viewLight`), reflection-probe проекция.

**Fragment shader (основной путь материалов):**
* `color = texture(tex0,v_tex0) * v_color`
* Солнце: `diffuseVec = diffuseCoef * diffuse * u_sunLightingColor` (константа
  `(0.2,0.2,0.2)` в куске), блик `pow(N·H,50)*0.4`, **envmap**:
  `reflect(view,normal)` → `pow(env,0.8)*fresnel(pow3.5, clamp 0.15..0.7)*u_envMapCoef`.
* `color.rgb += diffuseVec + specular + envmap`
* Тени: shadow-map (`sampler2DShadow tex5`, poisson 4 сэмпла, bias по касанию) с
  fallback `0.3*pow((1-diffuse),24)` — «brd”-тень, похожая на VC-овскую.
* Снег (`u_snow*`), терраин-слои (mask + 4 тайла), вода (refract/reflect + движение),
  небо (sky/sun/clouds цвета), PBR-emission (`u_pbrModelEmission*`), emissive-карта.
* **Гамма в материале:** `color.rgb = pow(color.rgb, u_localInvGamma)`.
* Туман: `mix(color.rgb, u_fogColor.rgb, v_fog)`.
* Lightmap: `mix(color, lightmap*tex*3, lightmap.a) * u_lightmapMulti`.
* Alpha test: порог `0.5` (и `u_smoothAlphaThreshold`).

### Прочие подсистемы (по GLSL/строкам)
Тени-картами (`renderer/Shadows.cpp`, `CastShadowEntityXY`, `StoreShadowForPed` —
VC-ский код + шейдерные upgrade), PostFX (`PostFX1ARGB/PostFX2ARGB`, color grade
`contrastMult/Add`, `u_colorInterp`), радар/карта (path/SDF/paint-шейдеры),
мультивью `GL_OVR_multiview` (стерео), UI-батчеры (`cbuffer0_vs`).

## 5. Что это значит для reVC

| Область | Статус |
|---|---|
| `.mod` нативное чтение | ✅ уже идентично клиенту (магия/ключ/TEA/блоки) |
| `.btx` | ✅ уже идентично (flags+KTX11/ASTC) |
| SA-скины/модели | ✅ стек совпадает (skin/hanim/GPU-skinning/ANPK) |
| Рендер-шейдеры | ⚠️ отличия: попиксельный солнечный свет+блик+envmap в материале, гамма, туман от глаза, uvMatrix, materialAmbient, shadow-map |
| Мировые эффекты (снег/вода/небо/терраин/PBR) | требуют данных BR; в VC-контенте смысла нет — адаптируются к имеющимся данным |

Реконструкция мировых шейдеров: `shaders_reconstructed/world.vert`, `world.frag`.
