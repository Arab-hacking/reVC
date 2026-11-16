# Шейдеры клиента: что найдено в архиве `shad_named`

Источник: `custom/zip/.../shad/` — 32 файла GLSL ES 3.00, снятые с живого
клиента (`fullscreen_quad`, `mesh_*`, `terrain_*`, `sky_clouds_*`, `water`,
`ui_vector_*`). Это эталон: порт должен повторять эти формулы, а не выводить
их заново.

## Затемнение в тени — главная находка

Все семь теневых шейдеров (`mesh_shadow_clip`, `mesh_shadow_clip_alphatest`,
`..._alphatest_smooth`, `mesh_shadow_fog`, `..._alphatest`,
`..._alphatest_smooth`, `terrain_shadow_clip`, `terrain_shadow_fog`) считают
тень **одной и той же формулой**:

```glsl
float visibility = 0.0;
for (int i = 0; i < 4; i++)
    visibility += 0.25 * texture(tex5, vec3(v_shadowProj.x + poissonDisk[i].x * 0.0005,
                                           v_shadowProj.y + poissonDisk[i].y * 0.0005,
                                           v_shadowProj.z));

shadow = 0.3 * (1.0 - visibility);
shadow = max(shadow, 0.3 * pow(1.0 - diffuse, 24.0));
color *= v_color;
color.rgb *= (1.0 - shadow);
```

Три следствия, которые надо держать в голове:

1. **Тень затемняет не более чем на 30%.** Это затемнение, а не отсутствие
   света. Порт раньше использовал `shadowVis` как множитель прямого света —
   полностью затенённая стена теряла солнце целиком и читалась сплошной
   чёрной плитой.
2. **`shadow = 0.3 * pow(1.0 - diffuse, 24.0)`** — terminator-поправка
   максимумом, а не добавкой.
3. **PCF: ровно 4 тапа, радиус 0.0005** в пространстве теневой карты, и
   `tex5` — `sampler2DShadow`, а не обычная текстура.

Биас считается в вершинном шейдере:

```glsl
float bias = max(0.01 * pow(1.0 - dot(normalize(Normal), normalize(-u_sunDir)), 4.0), 0.0001);
shadowProj.z -= bias;
shadowProj /= shadowProj.w;
v_shadowProj = shadowProj.xyz * 0.5 + 0.5;
```

## Какие объекты вообще получают тень

В архиве намеренно лежат и «теневые», и обычные варианты одних и тех же
мешей: `mesh_basic` против `mesh_shadow_fog`, `terrain_basic` против
`mesh_shadow_clip`, `sky_clouds_basic` против `sky_clouds_fog`. В `mesh_basic`
тени нет вовсе (`float shadow = 0.0;`). Вода (`water.fs`) и весь UI
(`ui_vector_*`) тени не получают.

То есть решение «рисовать ли тень на этом объекте» принимается выбором
шейдера, а не флагом внутри одного шейдера.

## Туман

Вершинный шейдер всех вариантов:

```glsl
v_fog = clamp((length(Vertex.xyz - u_eye.xyz) - u_fogData.x) * u_fogData.y, 0.0, 1.0);
```

`u_fogData.x` — начало тумана, `u_fogData.y` — `1.0 / (fogStart - fogEnd)`.
Фрагментный шейдер: `color.rgb = mix(color.rgb, u_fogColor.rgb, v_fog)`.

Это совпадает с `uniformState.fogRange = 1.0f/(rwStateCache.fogStart - rwStateCache.fogEnd)`
в `vendor/librw/src/gl/gl3device.cpp`, поэтому правка `fogEnd` на `FarClip` из
timecyc соответствует клиенту.

## Небо

`sky_clouds_fog.fs` — полная модель:

```glsl
mediump vec3 eyeToVertex = normalize(u_eye - v_worldPosition);
mediump vec3 normals = normalize(texture(tex1, v_tex0).rgb * 2.0 - 1.0);
mediump float cloudDensity = texture(tex0, v_tex0).g;
mediump float cloudLighting = max(dot(u_skySunDir, normals), 0.0);
mediump float blendSkyBottomTop = dot(eyeToVertex, vec3(0.0, 0.0, 1.0));
mediump vec3 color = mix(u_skyColor, u_skyTopColor, abs(blendSkyBottomTop));
color = mix(color, u_skyUnderColor, (clamp(blendSkyBottomTop, 0.0, 0.2) * 5.0));
highp float coef = max(dot(u_skySunDir, eyeToVertex), 0.0);
coef = max(coef - 0.995, 0.0) * 200.0;
mediump vec3 cloudColor = mix(u_cloudsColor.rgb, u_cloudsTopColor.rgb, cloudLighting);
color = mix(cloudColor, color, cloudDensity);
color = mix(color, u_sunColor, coef * cloudDensity);
```

Небо рисуется по сфере: смешивание нижнего и верхнего цвета по `dot(направление, Z)`,
плюс отдельная зона «под небом» и солнечный ореол с порогом 0.995.

## Постобработка — то, что делает картинку «красочной»

`sub_1557570_about_contrastMult` @ `0x01557570` регистрирует четыре варианта
фрагментного шейдера (с обводкой и без, с виньеткой и без). Общая часть:

```glsl
uniform mediump vec3 contrastMult;
uniform mediump vec3 contrastAdd;
uniform mediump vec3 vignetteParams;
uniform mediump vec4 vignetteColor;

mediump vec4 rast = texture(tex0, v_tex0);
rast.a = 1.0;
mediump float radius = sqrt(pow2(v_tex0.x - 0.5) * vignetteParams.x + pow2(v_tex0.y - 0.5));
mediump float fade = clamp(radius * vignetteParams.y + vignetteParams.z, 0.0, 1.0);
rast.rgb = mix(rast.rgb * contrastMult + contrastAdd, vignetteColor.rgb, fade * vignetteColor.a);
```

То есть весь «цвет» клиента — это **линейный контраст `rgb * mult + add`**,
флат по всему кадру, плюс необязательная виньетка. Порядок важен:
контраст, потом виньетка, поверх уже готового кадра.

`contrastMult`/`contrastAdd` приходят из ассетов (Unity-сериализация через
`FUN_0166cb98("contrastMult", 4, 1)`), числовых литералов в псевдокоде нет —
их надо брать из данных, а не выдумывать.

Порт уже имеет 3×4 матрицу `u_colorGrade` из `data/colorcycle.dat` с тем же
смыслом (это `rgb * M`), но без `contrastAdd` и без виньетки.

## Чего в архиве нет

- Теней от больших зданий (`bIsBIGBuilding`): в шейдерах нет отдельной ветки.
- Терминаторной кривой, отличной от `pow(1 - diffuse, 24)`.
- Тени на воде, небе без облаков и UI.

## Как это сверять

Значения `0.3`, `0.0005`, `0.995`, `200.0`, `0.01`, `0.0001` — литералы из
файлов, их можно цитировать как есть. Всё, что приходит юниформами
(`contrastMult`, `vignetteParams`, `u_fogData`), надо брать из данных игры.
