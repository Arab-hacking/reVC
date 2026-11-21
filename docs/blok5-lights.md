# Блок 5 — световые текстуры ламп и порт оптики из San Andreas

Ветка `arena/782fc865-revc`. Изменено 15 файлов, +1176 / −648.
Полный прогон: **267 файлов, FAIL=0** в трёх конфигурациях (по умолчанию,
`-DFIX_BUGS -DSA_VEHICLE_MODELS`, `-DCUSTOM_MODELS`). Дополнительно 10
изменённых `.cpp` собраны в объектники (`-c -O1`) — ошибок нет.
Эталон: `gta-reversed` (`/tmp/gtr`); адреса статиков — plugin-sdk (`/tmp/psdk`).

---

## 1. Что было не так

Свет фар в reVC (VC-наследство) жил двумя независимыми блоками внутри
`CAutomobile::PreRender` и одним своим блоком в `CBike::PreRender`. Каждый блок
сам решал, когда включать фары, сам выбирал цвета и размеры корон, сам проверял
повреждения — и **ни один из них ничего не записывал** в состояние машины.
Последствия, все наблюдаемые:

| Симптом | Причина |
|---|---|
| Лампы на модели никогда не «зажигаются», горит только корона | не было `ms_lightsOn`/`ms_pLightsTexture` и прохода по материалам |
| Одна фара синяя, другая жёлтая при одинаковом расчёте | VC передаёт `RegisterCorona(r, r, b)`, SA — `(r, r, g)` |
| Короны неестественно большие у капота | VC берёт `size` линейно от косинуса, SA — от `sqrt(cos)` |
| Вторая пара фар (`headlights2`) не светит | dummy никогда не читался |
| `CBike::Render` не рисует конус (`DoHeadLightBeam`) | условие `m_renderLights.m_bRightFront` не могло стать true |
| Задние фонари при выключенных фарах ведут себя как стопы | не было разделения «динамическая»/«статическая» часть |
| Задний ход светится красным | не было `DoReverseLightEffect` |
| Пятно света на дороге одно и несимметричное | не было `DoHeadLightReflection` с twin/single |
| Скриптовое `FORCE_CAR_LIGHTS_ON` невозможно | не было `m_nOverrideLights` |

## 2. Что сделано

### 2.1 `CVehicleModelInfo` — editable-материалы и лампы

Новый SA-путь целиком (`src/modelinfo/VehicleModelInfo.{h,cpp}`):

* статики `ms_pLightsTexture` / `ms_pLightsOnTexture` (0xB4E68C/0xB4E690) и
  `ms_lightsOn[4]` (0xB4E3E8);
* `SetupLightFlags(CVehicle*)` (0x4C8C90) — переносит `m_renderLights` в
  `ms_lightsOn`;
* `SetEditableMaterials` / `RestoreEditableMaterials` (0x4C8430 / 0x4C8460)
  с общим списком восстановления: `RemapEntry` теперь помнит не только
  текстуру, но и цвет и `RwSurfaceProperties`; лимит поднят с 48 до SA-шных
  256 (`gRestoreEntries`);
* `SetEditableMaterialsCB` (0x4C8220) в SA-порядке: paintjob `'#'` → лампа
  (маркер-цвет → белый, при `ms_lightsOn[slot]` → `ms_pLightsOnTexture` +
  `gLightSurfProps`) → иначе тело не трогается;
* `LoadLightTextures()` — резолв двух листов из `vehicle.txd`;
* `ShutdownLightTexture()` (0x4C7470);
* `IsLightsMaterial()` — по указателю SA-текстуры **и** по имени
  (`vehiclelights128`/`vehiclelightson128`), потому что конвертированная
  модель может нести копию листа в своём TXD.

`src/extras/custom/VehicleMaterialClassify.h`: `IsVehicleLightMarker(r,g,b,slot)`
возвращает слот 0..3 (LF/RF/LR/RR), старая трёхаргументная версия осталась
обёрткой; добавлен `IsVehicleLightsTextureName`.

`src/extras/custom/CustomModels.cpp`: `LoadLightTextures()` вызывается в
`EnsureVehicleTxdLoaded` дважды — сразу после загрузки `vehicle.txd` (как SA в
`SetupCommonData`) и в конце функции, после добавления BR-ресурсов: лист,
которого не было в словаре в первый раз, иначе остался бы невидимым навсегда.

`src/rw/TxdStore.cpp`: `ForgetCachedLightTextures(slot)` сбрасывает оба
указателя, когда уничтожается именно vehicle-словарь (`RemoveTxdSlot`,
`RemoveTxd`) — иначе после перезагрузки игры или подмены кастомного
`vehicle.txd` материал сравнивал бы текстуры с освобождённой памятью.

### 2.2 `CVehicle` — весь световой проход

Блок света перенесён из `CAutomobile` в `CVehicle` (в SA он тоже базовый) и
переписан по эталону:

* `DoVehicleLights(CMatrix&, eVehicleLightsFlags)` (0x6E1A60) — единственное
  место, где решается судьба фар: время суток/погода/зона
  (`GetVehicleLightsStatus`, 0x6D55C0), скриптовый override
  (`m_nOverrideLights`), мигание сигнализации, выключенный двигатель,
  всплывающие фары `MI_INFERNUS`, проверка разбитых лампы через
  `GetDamageManager()`, затем `RenderLights`-логика: MAIN + SECONDARY,
  TWIN/одинарная пара, тыл с `staticEmission`, `DoReverseLightEffect`,
  `DoHeadLightReflection`, два `CPointLights::AddLight`;
* `DoLightEffect` (0x6E1780) — одна лампа: dummy, `+0.05*fwd` для переда,
  зеркалирование левой стороны по `dummyPos.x`, `normAngle = sqrt(cos)`,
  `intensity/size` SA-константами, поездной множитель, «тонкая линия»
  headlight-line, красный тыл 128 при торможении / 96 при `staticEmission`;
* `DoHeadLightReflection` (0x6E1720) — пятно на дороге, twin/single, выбор
  `gpShadowHeadLightsTex` / `gpShadowHeadLightsTex2`;
* `DoHeadLightBeam(eVehicleLightId, CMatrix&, bool)` — получил id лампы и
  выбирает dummy через `GetDummyFromLightId`;
* `GetVehicleLightsFlags()`, `GetDummyFromLightId()`
  (DUMMY_LIGHT_FRONT/REAR_MAIN/SECONDARY);
* `bForceVehicleLightsOff` — SA `ms_forceVehicleLightsOff`, статик на
  `CVehicle`;
* `SetupRender()` / `ResetAfterRender()` (0x6D64F0 / 0x6D0E20) — порядок SA:
  `SetupLightFlags` → `SetVehicleColour(4)` → `ApplyRemapTexture` →
  `SetEditableMaterials`, сброс — `RestoreEditableMaterials`;
* `GetDamageManager()` — виртуальный (в `Vehicle.h` с `class CDamageManager *`,
  потому что тип ещё не объявлен), по умолчанию `nil`; `CAutomobile`
  переопределяет (`&Damage`) и получает типизированный `GetDamage()`;
* новый геттер/сеттер `GetOverrideLights`/`SetOverrideLights` — состояние
  скриптового приказа стало доступно снаружи, как и остальной API Блока 4.

### 2.3 Отрисовка

`SetupRender`/`ResetAfterRender` стоят **вне** `Render` — как в SA — потому что
`CRenderer::RenderOneNonRoad` рисует сортированные alpha-атомики машины уже
после возврата из `Render`, и подмена материалов должна жить до этого момента:

* `src/renderer/Renderer.cpp` — пара вокруг отрисовки машины в
  `RenderOneNonRoad` (0x553260);
* `src/renderer/SunShadowMap.cpp` — та же пара в шэдоу-пассе: он рисует те же
  материалы, и «забытый» своп был бы виден в основном проходе;
* `CAutomobile::Render` и `CBike::Render` больше не трогают ни цвета, ни
  paintjob, ни материалы (в `CBike::Render` остался только
  `DoHeadLightBeam(VEHICLE_LIGHT_MAIN, m_leanMatrix, true)`);
* `CAutomobile::PreRender` и `CBike::PreRender` вызывают
  `DoVehicleLights(...)`; у мотоцикла — после `CalculateLeanMatrix()`, по
  матрице наклона (фара висит на вилке), `PreRender` мотоцикла стал короче на
  224 строки.

### 2.4 Текстуры

`gpShadowHeadLightsTex2` (`"headlight1"`) добавлена в
`src/renderer/Shadows.{h,cpp}`. В VC-установке такой текстуры нет, поэтому
после `RwTextureRead` стоит фолбэк на `gpShadowHeadLightsTex`, а в
`Shutdown` — проверка `!= gpShadowHeadLightsTex` перед `RwTextureDestroy`
(иначе двойное освобождение одной текстуры).

`CVehicleModelInfo::ShutdownLightTexture()` вызывается из `CGame::ShutDown`
рядом с `ShutdownEnvironmentMaps`, до `CTxdStore::GameShutdown` — SA делает это
в том же месте.

## 3. Приближения (и почему)

1. **`gLightSurfProps`** — значения в бинаре не документированы. Взято
   `{ ambient 1.0f, specular 0.0f, diffuse 1.0f }`: лампа светит своим листом
   и не должна затеняться сценой и не должна бликовать. Пометка стоит в коде
   (`GetLightsSurfaceProperties`).
2. **Тёмная зона в `GetVehicleLightsStatus`** — SA смотрит четыре угольных
   сэмпла освещения столкновений и `CCullZones`; в reVC такого сэмпла нет.
   Приближение: `CCullZones::CamNoRain()` + средняя ambient от
   `CTimeCycle::GetAmbientRed/Green/Blue()` < 0.05 — ровно та «поверхность
   темнее пяти процентов», о которой говорит эталон.
3. **Гангстеры не включают фары** — SA требует ещё и гангстерскую зону
   (`CPopCycle::m_bCurrentZoneIsGangArea`); в VC-модели населения такого
   признака нет, остались тип педа + чётность seed.
4. **TWIN-флаг** — в handling-флагах SA нет «twin lights»; вместо этого
   проверяется наличие dummy `DUMMY_LIGHT_FRONT_MAIN` у модели
   (`IsDummyActive`), и флаг не ставится мотоциклам/BMX.
5. **`MI_QUAD`** — в `ModelIndices.h` такого id нет; используется
   `IsSubQuad()` из handling-записи. `MI_TRAM` (исключение в SA-шном
   `isFullsizeTrain`) тоже отсутствует — трамвая в VC нет.
6. **`DoReverseLightEffect`** в эталоне не реверснут — реализовано по смыслу
   (белая пара типа STAR, только при включённых фарах и только когда камера
   сзади).

## 4. Проверка

```
267 файлов × 3 конфигурации: FAIL=0
10 изменённых .cpp: -c -O1 -DLIBRW -DRW_GL3: COMPILE_FAIL=0
```

Новых предупреждений в затронутых файлах нет (остались только прежние
`-Wparentheses`/`-Wswitch` по существующему коду).

## 5. Полный дифф

```diff
diff --git a/src/core/Game.cpp b/src/core/Game.cpp
index a629b0a..23caf80 100644
--- a/src/core/Game.cpp
+++ b/src/core/Game.cpp
@@ -731,6 +731,10 @@ bool CGame::ShutDown(void)
 	CAnimManager::Shutdown();
 	CVehicleModelInfo::DeleteVehicleColourTextures();
 	CVehicleModelInfo::ShutdownEnvironmentMaps();
+	// The lamp sheets are textures of the vehicle dictionary, which
+	// CTxdStore::GameShutdown below tears down; SA destroys them in the same
+	// place (CVehicleModelInfo::ShutdownLightTexture).
+	CVehicleModelInfo::ShutdownLightTexture();
 	CRadar::Shutdown();
 	CStreaming::Shutdown();
 #ifdef CUSTOM_MODELS
diff --git a/src/extras/custom/CustomModels.cpp b/src/extras/custom/CustomModels.cpp
index 3960fc3..ef79999 100644
--- a/src/extras/custom/CustomModels.cpp
+++ b/src/extras/custom/CustomModels.cpp
@@ -5163,6 +5163,14 @@ CCustomModels::EnsureVehicleTxdLoaded(void)
 				CUSTOM_LOG("vehicle.txd: loaded from models/generic/vehicle.txd\n");
 		}
 	}
+
+	// SA CVehicleModelInfo::SetupCommonData (0x4C75E0) resolves the two lamp
+	// sheets the moment the dictionary is in hand, because the material pass
+	// compares texture pointers against them. The dictionary here may have come
+	// from a zip, a loose folder, an archive or the IMG, and the BR resources
+	// below can add the sheets to it afterwards, so the lookup is repeated at
+	// the end of this function (see below) as well.
+	CVehicleModelInfo::LoadLightTextures();
 	if(def->texDict == nil)
 		CTxdStore::Create(slot);
 	if(def->refCount == 0)
@@ -5273,6 +5281,12 @@ CCustomModels::EnsureVehicleTxdLoaded(void)
 				RwTexDictionaryAddTexture(def->texDict, specFallback);
 		}
 	}
+
+	// The BR lamp resources were added to the dictionary above, and a texture
+	// that was not in it the first time can only be found now. Resolving again
+	// costs two dictionary lookups and is what makes the lamp sheet swap work on
+	// a BR install that keeps vehiclelights128 in a standalone archive.
+	CVehicleModelInfo::LoadLightTextures();
 }
 
 void
diff --git a/src/extras/custom/VehicleMaterialClassify.h b/src/extras/custom/VehicleMaterialClassify.h
index 9ccab01..59ff815 100644
--- a/src/extras/custom/VehicleMaterialClassify.h
+++ b/src/extras/custom/VehicleMaterialClassify.h
@@ -65,12 +65,62 @@ inline bool IsBRBodyRemap(const char *name)
 	       std::strncmp(name, "remaptoner", 10) != 0;
 }
 
+// SA CVehicleModelInfo::SetEditableMaterialsCB (0x4C8220) recognises a lamp
+// material by its texture - "vehiclelights128" - and then by one of exactly
+// four RGBA colours, one per lamp of the car, in this order:
+//
+//   slot 0  (255, 175,   0)  left  front lamp   (headlights)
+//   slot 1  (  0, 255, 200)  right front lamp   (headlights)
+//   slot 2  (185, 255,   0)  left  rear  lamp   (taillights)
+//   slot 3  (255,  60,   0)  right rear  lamp   (taillights)
+//
+// These four numbers are the whole SA lamp vocabulary: brake lights, position
+// lights and reversing lights are not separate materials in SA, they are extra
+// lit quads inside the same four lamp textures (a car model paints the brake
+// quad with the same taillight colour, and the game lights it by switching the
+// whole material to vehiclelightson128 and switching the *corona* instead).
+// A model with two lamps per side therefore repeats the same four colours in
+// its second lamp quad, which is what the "headlights2"/"taillights2" dummies
+// are for - the geometry is separate, the material identity is not.
+//
+// The `slot` output is the index into CVehicleModelInfo::ms_lightsOn[]:
+// 0 left front, 1 right front, 2 left rear, 3 right rear.
+inline bool IsVehicleLightMarker(uint8_t r, uint8_t g, uint8_t b, int &outSlot)
+{
+	if(r == 255 && g == 175 && b == 0){ outSlot = 0; return true; }
+	if(r == 0 && g == 255 && b == 200){ outSlot = 1; return true; }
+	if(r == 185 && g == 255 && b == 0){ outSlot = 2; return true; }
+	if(r == 255 && g == 60 && b == 0){ outSlot = 3; return true; }
+	outSlot = -1;
+	return false;
+}
+
 inline bool IsVehicleLightMarker(uint8_t r, uint8_t g, uint8_t b)
 {
-	return (r == 255 && g == 175 && b == 0) ||
-	       (r == 0 && g == 255 && b == 200) ||
-	       (r == 185 && g == 255 && b == 0) ||
-	       (r == 255 && g == 60 && b == 0);
+	int slot;
+	return IsVehicleLightMarker(r, g, b, slot);
+}
+
+// A material belongs to the lamp sheet when it wears one of the two lamp
+// textures. SA compares the texture pointer against ms_pLightsTexture; the name
+// is checked as well because a converted model can carry its own copy of
+// "vehiclelights128" inside the car's TXD instead of pointing at the one in
+// vehicle.txd, and that copy is still a lamp sheet.
+inline bool IsVehicleLightsTextureName(const char *texName)
+{
+	if(!texName || !*texName)
+		return false;
+	static const char *lampSheets[] = { "vehiclelights128", "vehiclelightson128" };
+	for(const char *name : lampSheets){
+		size_t n = std::strlen(name);
+		size_t i = 0;
+		while(i < n && texName[i] &&
+		      std::tolower((unsigned char)texName[i]) == std::tolower((unsigned char)name[i]))
+			i++;
+		if(i == n && texName[i] == '\0')
+			return true;
+	}
+	return false;
 }
 
 inline bool IsTireMaterial(const char *frameName, const char *texName)
diff --git a/src/modelinfo/VehicleModelInfo.cpp b/src/modelinfo/VehicleModelInfo.cpp
index 527be92..00bb513 100644
--- a/src/modelinfo/VehicleModelInfo.cpp
+++ b/src/modelinfo/VehicleModelInfo.cpp
@@ -42,6 +42,9 @@ RwTexture *CVehicleModelInfo::ms_colourTextureTable[256];
 CVehicleModelInfo::CLinkedUpgradeList CVehicleModelInfo::ms_linkedUpgrades;
 uint8 CVehicleModelInfo::ms_currentCol[NUM_CURRENT_COLORS] = { 0, 0, 0, 0 };
 RwTexture *CVehicleModelInfo::ms_pRemapTexture;
+RwTexture *CVehicleModelInfo::ms_pLightsTexture;
+RwTexture *CVehicleModelInfo::ms_pLightsOnTexture;
+uint8 CVehicleModelInfo::ms_lightsOn[NUM_LIGHTS] = { 0, 0, 0, 0 };
 int16 CVehicleModelInfo::ms_numWheelUpgrades[NUM_WHEELS];
 int16 CVehicleModelInfo::ms_upgradeWheels[NUM_WHEELS][NUM_WHEEL_UPGRADES];
 int32 CVehicleModelInfo::ms_wheelFrameIDs[NUM_WHEELS] = {
@@ -2557,31 +2560,165 @@ CVehicleModelInfo::ChooseVehicleColour(uint8 &col1, uint8 &col2, uint8 &col3, ui
 }
 
 // ---------------------------------------------------------------------------
-// SA paintjobs: swapping the body texture for a frame
+// Editable materials: the paintjob and the lamps
 // ---------------------------------------------------------------------------
-
-// SA CVehicleModelInfo::SetEditableMaterialsCB (0x4C8AB0).
 //
-// A car's body texture is named with a leading '#': the game does not use the
-// image the material points at, it replaces it with the paintjob of the vehicle
-// being drawn. Anything else - glass, the plate, the lights - keeps its own
-// texture.
+// SA CVehicleModelInfo::SetEditableMaterialsCB (0x4C8220).
+//
+// One callback, two jobs, in this order:
+//
+//  1. A body material - its texture name starts with '#' - is the car's skin.
+//     The image is not in the model, it is the paintjob dictionary of the car
+//     being drawn, so the material is pointed at it.
+//  2. A lamp material - it wears one of the two lamp sheets - carries state,
+//     not paint: its RGBA is a marker that says which of the four lamps this
+//     quad is, and the game has to answer with the lit sheet or the dark one.
+//     The marker colour is never meant to be seen, so it is reset to white and
+//     the sheet itself provides the colour.
+//
+// Anything else - glass, the plate, interior - is left alone.
+//
+// Every write is recorded in the model's restore list. The two jobs share the
+// list because they share the material: a bumper with a lamp quad on it is both
+// a body material and a lamp material, which is exactly what SA does too (it
+// checks the paintjob condition first and falls through to the lamp condition
+// afterwards, so such a material gets both writes).
+
+// The three surface properties SA copies over a lit lamp material
+// (gLightSurfProps at 0x8A645C). Full ambient: a lamp is a light source of its
+// own and must not be shaded by the scene, so the material keeps its sheet at
+// full brightness. No specular: a lamp lens is not a mirror, and letting the
+// sun reflect off it is what makes an unlit car look wet. Full diffuse, so the
+// last-listed lighting component does not dim the sheet either.
+//
+// SA keeps a single instance of this structure and copies it wholesale; the
+// exact numbers are not recoverable from the binary beyond "the lamp stays at
+// its sheet brightness", which is what the three values below produce.
+const RwSurfaceProperties &
+CVehicleModelInfo::GetLightsSurfaceProperties(void)
+{
+	static const RwSurfaceProperties props = { 1.0f, 0.0f, 1.0f };
+	return props;
+}
+
+bool
+CVehicleModelInfo::IsLightsMaterial(RpMaterial *material)
+{
+	RwTexture *texture = RpMaterialGetTexture(material);
+	if(texture == nil)
+		return false;
+	// SA compares the texture pointer against the one it resolved from
+	// vehicle.txd. A converted car can carry its own copy of the sheet inside
+	// its own TXD, so the name is checked as well.
+	if(ms_pLightsTexture && texture == ms_pLightsTexture)
+		return true;
+	if(ms_pLightsOnTexture && texture == ms_pLightsOnTexture)
+		return true;
+	return customveh::IsVehicleLightsTextureName(RwTextureGetName(texture));
+}
+
 RpMaterial *
 CVehicleModelInfo::SetEditableMaterialsCB(RpMaterial *material, void *data)
 {
 	CVehicleModelInfo *mi = (CVehicleModelInfo*)data;
-	RwTexture *texture = RpMaterialGetTexture(material);
-	if(texture == nil || RwTextureGetName(texture) == nil || RwTextureGetName(texture)[0] != '#')
+	if(material == nil || mi == nil)
 		return material;
-	if(ms_pRemapTexture == nil)
-		return material;
-	if(mi->m_nNumRemapEntries >= NUM_REMAP_ENTRIES)
+
+	const RwRGBA *color = RpMaterialGetColor(material);
+	RwTexture *texture = RpMaterialGetTexture(material);
+
+	// 1. The paintjob. SA tests the '#' before anything else, and does not
+	// make the swap conditional on the material not being a lamp: a '#'
+	// texture is a skin, and a skin is never a lamp sheet.
+	if(ms_pRemapTexture && texture && RwTextureGetName(texture) &&
+	   RwTextureGetName(texture)[0] == '#'){
+		if(mi->m_nNumRemapEntries < NUM_REMAP_ENTRIES){
+			RemapEntry &entry = mi->m_aRemapEntries[mi->m_nNumRemapEntries++];
+			entry.material = material;
+			entry.texture = texture;
+			entry.swappedTexture = 1;
+			entry.changedColour = 0;
+			entry.changedSurfaceProps = 0;
+			RpMaterialSetTexture(material, ms_pRemapTexture);
+		}
+		// Re-read: the swap above already changed what the material wears,
+		// and the lamp job below must look at the *new* texture, as SA does.
+		texture = RpMaterialGetTexture(material);
+	}
+
+	// 2. The lamps.
+	if(IsLightsMaterial(material)){
+		int lightIndex = -1;
+		const bool isMarker = customveh::IsVehicleLightMarker(color->red, color->green,
+			color->blue, lightIndex);
+
+		// The marker colour is a label for the modeller, not the lamp colour:
+		// it is cleared to white for every lamp material, marker or not, so
+		// that a model which painted its lamp quads with an arbitrary colour
+		// still gets an untinted sheet. A marker is additionally what chooses
+		// the lamp slot whose flag decides whether the sheet is lit.
+		if(color->red != 255 || color->green != 255 || color->blue != 255){
+			if(mi->m_nNumRemapEntries < NUM_REMAP_ENTRIES){
+				RemapEntry &entry = mi->m_aRemapEntries[mi->m_nNumRemapEntries++];
+				entry.material = material;
+				entry.colour = *color;
+				entry.changedColour = 1;
+				entry.swappedTexture = 0;
+				entry.changedSurfaceProps = 0;
+			}
+			RwRGBA white = { 255, 255, 255, color->alpha };
+			RpMaterialSetColor(material, &white);
+		}
+
+		// Only a marker names a lamp of the car, so only a marker can be lit.
+		// A lamp sheet used for something that is not one of the four lamps
+		// (an indicator strip painted on the same sheet, a taxi sign) stays
+		// dark, which is what SA does by leaving iLightIndex at -1.
+		if(isMarker && lightIndex >= 0 && lightIndex < NUM_LIGHTS &&
+		   ms_lightsOn[lightIndex] && ms_pLightsOnTexture){
+			// Two fields may need restoring - the sheet and the surface
+			// properties - so the room for both is checked together: a
+			// half-swapped lamp would stay lit after the car is done.
+			if(mi->m_nNumRemapEntries + 2 > NUM_REMAP_ENTRIES)
+				return material;
+
+			RwTexture *currentTexture = RpMaterialGetTexture(material);
+			const RwSurfaceProperties &props = GetLightsSurfaceProperties();
+			const RwSurfaceProperties *oldProps = RpMaterialGetSurfaceProperties(material);
+			const bool propsDiffer = oldProps == nil ||
+				oldProps->ambient != props.ambient ||
+				oldProps->specular != props.specular ||
+				oldProps->diffuse != props.diffuse;
+
+			if(currentTexture != ms_pLightsOnTexture){
+				RemapEntry &entry = mi->m_aRemapEntries[mi->m_nNumRemapEntries++];
+				entry.material = material;
+				entry.texture = currentTexture;
+				entry.swappedTexture = 1;
+				entry.changedColour = 0;
+				entry.changedSurfaceProps = 0;
+				RpMaterialSetTexture(material, ms_pLightsOnTexture);
+			}
+			if(propsDiffer){
+				RemapEntry &entry = mi->m_aRemapEntries[mi->m_nNumRemapEntries++];
+				entry.material = material;
+				entry.surfaceProps = *oldProps;
+				entry.changedSurfaceProps = 1;
+				entry.swappedTexture = 0;
+				entry.changedColour = 0;
+				RpMaterialSetSurfaceProperties(material, &props);
+			}
+		}
 		return material;
+	}
 
-	mi->m_aRemapEntries[mi->m_nNumRemapEntries].material = material;
-	mi->m_aRemapEntries[mi->m_nNumRemapEntries].texture = texture;
-	mi->m_nNumRemapEntries++;
-	RpMaterialSetTexture(material, ms_pRemapTexture);
+	// 3. Not a lamp, so it can only be one of the four body colour markers.
+	// The marker was already turned white by FindEditableMaterialList, which is
+	// what keeps the colour of a material that is later repainted from being
+	// lost; SA instead resolves the colour here, through ms_currentCol, and
+	// does it for every material it walks rather than for a list built when the
+	// model was loaded. Both end up painting every body marker with the colour
+	// of the slot it names, which is why reVC keeps its own path.
 	return material;
 }
 
@@ -2589,11 +2726,17 @@ RpAtomic *
 CVehicleModelInfo::SetEditableMaterialsAtomicCB(RpAtomic *atomic, void *data)
 {
 	CVehicleModelInfo *mi = (CVehicleModelInfo*)data;
+	if(atomic == nil || RpAtomicGetGeometry(atomic) == nil)
+		return atomic;
+	// SA skips an atomic that is not being drawn: its materials are not on
+	// screen and swapping them would only be undone again.
+	if((RpAtomicGetFlags(atomic) & rpATOMICRENDER) == 0)
+		return atomic;
 	RpGeometryForAllMaterials(RpAtomicGetGeometry(atomic), SetEditableMaterialsCB, mi);
 	return atomic;
 }
 
-// SA CVehicleModelInfo::SetEditableMaterials (0x4C8B60).
+// SA CVehicleModelInfo::SetEditableMaterials (0x4C8430).
 void
 CVehicleModelInfo::SetEditableMaterials(void)
 {
@@ -2603,15 +2746,85 @@ CVehicleModelInfo::SetEditableMaterials(void)
 	RpClumpForAllAtomics(m_clump, SetEditableMaterialsAtomicCB, this);
 }
 
-// The other half: put every swapped material's own texture back. Called once
-// the car has been drawn, so the model - which is shared by every car of this
-// model - is left exactly as it was found.
+// SA CVehicleModelInfo::ResetEditableMaterials (0x4C8460). The other half:
+// every field the callbacks wrote goes back, in reverse order, so that a
+// material touched twice (paintjob then lamp, colour then surface properties)
+// ends up exactly as it was found. The model is shared by every vehicle of that
+// model, so this is what stops one car's paintjob and lit lamps from being
+// inherited by the next.
 void
 CVehicleModelInfo::RestoreEditableMaterials(void)
 {
-	for(int32 i = 0; i < m_nNumRemapEntries; i++)
-		RpMaterialSetTexture(m_aRemapEntries[i].material, m_aRemapEntries[i].texture);
+	for(int32 i = m_nNumRemapEntries - 1; i >= 0; i--){
+		RemapEntry &entry = m_aRemapEntries[i];
+		if(entry.material == nil)
+			continue;
+		if(entry.swappedTexture)
+			RpMaterialSetTexture(entry.material, entry.texture);
+		if(entry.changedColour)
+			RpMaterialSetColor(entry.material, &entry.colour);
+		if(entry.changedSurfaceProps)
+			RpMaterialSetSurfaceProperties(entry.material, &entry.surfaceProps);
+	}
 	m_nNumRemapEntries = 0;
+	// The static lamp flags belong to the vehicle that was just drawn, not to
+	// the model: clear them so a second Render of the same model - the alpha
+	// pass, a mirror - does not light lamps the vehicle no longer has on.
+	for(int32 i = 0; i < NUM_LIGHTS; i++)
+		ms_lightsOn[i] = 0;
+}
+
+// SA CVehicleModelInfo::SetupLightFlags (0x4C8C90). The four flags are read by
+// SetEditableMaterialsCB through ms_lightsOn, which is why they are copied out
+// of the vehicle instead of being handed to the callback: SA's callback takes
+// only the material and a restore-list pointer.
+void
+CVehicleModelInfo::SetupLightFlags(CVehicle *vehicle)
+{
+	if(vehicle == nil){
+		for(int32 i = 0; i < NUM_LIGHTS; i++)
+			ms_lightsOn[i] = 0;
+		return;
+	}
+	ms_lightsOn[0] = vehicle->m_renderLights.m_bLeftFront;
+	ms_lightsOn[1] = vehicle->m_renderLights.m_bRightFront;
+	ms_lightsOn[2] = vehicle->m_renderLights.m_bLeftRear;
+	ms_lightsOn[3] = vehicle->m_renderLights.m_bRightRear;
+}
+
+// SA resolves both lamp sheets once, from vehicle.txd, in SetupCommonData
+// (0x4C75E0). reVC's dictionary is built by CCustomModels, so the lookup is
+// repeated whenever that code has finished with it: a dictionary that carried
+// no lamp sheets the first time (BR ships them as standalone archives) must not
+// leave the swap permanently off.
+void
+CVehicleModelInfo::LoadLightTextures(void)
+{
+	int slot = CTxdStore::GetVehicleTxdSlot();
+	if(slot < 0)
+		slot = CTxdStore::FindTxdSlot("vehicle");
+	if(slot < 0)
+		return;
+	TxdDef *def = CTxdStore::GetSlot(slot);
+	if(def == nil || def->texDict == nil)
+		return;
+
+	ms_pLightsTexture = RwTexDictionaryFindNamedTexture(def->texDict, "vehiclelights128");
+	ms_pLightsOnTexture = RwTexDictionaryFindNamedTexture(def->texDict, "vehiclelightson128");
+}
+
+// SA CVehicleModelInfo::ShutdownLightTexture (0x4C7470).
+void
+CVehicleModelInfo::ShutdownLightTexture(void)
+{
+	if(ms_pLightsTexture){
+		RwTextureDestroy(ms_pLightsTexture);
+		ms_pLightsTexture = nil;
+	}
+	if(ms_pLightsOnTexture){
+		RwTextureDestroy(ms_pLightsOnTexture);
+		ms_pLightsOnTexture = nil;
+	}
 }
 
 // SA CVehicleModelInfo::SetRenderPipelines (0x4C8900).
diff --git a/src/modelinfo/VehicleModelInfo.h b/src/modelinfo/VehicleModelInfo.h
index ee452fa..719ad4f 100644
--- a/src/modelinfo/VehicleModelInfo.h
+++ b/src/modelinfo/VehicleModelInfo.h
@@ -192,10 +192,13 @@ enum {
 	NUM_VEHICLE_UPGRADE_SLOTS = 15,
 	NUM_MODEL_REMAPS   = 4,  // a model wears at most four paintjobs
 	NUM_CURRENT_COLORS = 4,  // SA renders cars with four colour slots
+	NUM_LIGHTS         = 4,  // SA ms_lightsOn: LF, RF, LR, RR
 	NUM_WHEELS         = 4,  // SA numbers wheel sets, not individual wheels
 	NUM_WHEEL_UPGRADES = 15  // wheel models per set (one per rim design)
 };
 
+class CVehicle;
+
 class CVehicleModelInfo : public CClumpModelInfo
 {
 public:
@@ -335,6 +338,21 @@ public:
 	// applied, and the wheel upgrade / frame tables the wheel code reads back.
 	static uint8 ms_currentCol[NUM_CURRENT_COLORS];
 	static RwTexture *ms_pRemapTexture;
+	// SA CVehicleModelInfo::ms_pLightsTexture / ms_pLightsOnTexture
+	// (0xB4E68C / 0xB4E690). The two lamp sheets of vehicle.txd: the dark one
+	// every car model paints its lamps with, and the lit one a lamp material is
+	// swapped to while that lamp is on. Both are resolved once, when
+	// vehicle.txd has been loaded, and stay nil when the dictionary has no such
+	// texture - the swap is then skipped and the lamps keep their material
+	// colour, which is what a VC-era car without lamp textures wants.
+	static RwTexture *ms_pLightsTexture;
+	static RwTexture *ms_pLightsOnTexture;
+	// SA CVehicleModelInfo::ms_lightsOn[4] (0xB4E3E8). Which of the four lamp
+	// slots of the car being drawn are lit: 0 left front, 1 right front,
+	// 2 left rear, 3 right rear. CVehicle::SetupLightFlags fills this from the
+	// vehicle's m_renderLights before the clump is drawn, and
+	// SetEditableMaterials reads it per material.
+	static uint8 ms_lightsOn[NUM_LIGHTS];
 	static int16 ms_numWheelUpgrades[NUM_WHEELS];
 	static int16 ms_upgradeWheels[NUM_WHEELS][NUM_WHEEL_UPGRADES];
 	static int32 ms_wheelFrameIDs[NUM_WHEELS];
@@ -494,34 +512,85 @@ public:
 	static RpAtomic *GetEditableMaterialListCB(RpAtomic *atomic, void *data);
 	void FindEditableMaterialList(void);
 
-	// Paintjob support. While a car with a remap texture is being drawn, its
-	// body materials are pointed at ms_pRemapTexture and put back afterwards;
-	// SA does exactly this with SetEditableMaterials/RestoreEditableMaterials
-	// and keeps the restore entries on the stack. reVC keeps the list of
-	// swapped materials in the model, because a Render is not always bracketed
-	// by the same call (the sorted alpha list may draw a car later).
+	// ---- Editable materials: paintjob and lamps -------------------------
+	//
+	// SA CVehicleModelInfo::SetEditableMaterials (0x4C8430) walks the clump
+	// once per drawn vehicle and rewrites the materials that carry state which
+	// is not in the model:
+	//
+	//   * a body material (its texture name starts with '#') is pointed at the
+	//     paintjob texture of the car being drawn;
+	//   * a lamp material (its texture is one of the two lamp sheets) has its
+	//     RGBA reset to white, so the four marker colours used by the modeller
+	//     to say "this is the left front lamp" do not tint the sheet, and is
+	//     then swapped to the lit sheet when that particular lamp is on.
 	//
-	// A body material is one whose texture name starts with '#' - SA's marker
-	// for "this material wears the car's paintjob, not a fixed image".
+	// Everything that was written is recorded so that
+	// ResetEditableMaterials (0x4C8460) can put the model back: the model is
+	// shared by every vehicle of that model, and one car's headlights must not
+	// stay on its next owner's.
+	//
+	// SA keeps the restore list on the stack (gRestoreEntries) and reuses it
+	// for everything the callbacks changed, the body colour included. reVC
+	// keeps a list per model instead, because a Render is not always bracketed
+	// by the same call: the sorted alpha list can draw a vehicle later.
 	void SetEditableMaterials(void);
 	void RestoreEditableMaterials(void);
 	static RpMaterial *SetEditableMaterialsCB(RpMaterial *material, void *data);
 	static RpAtomic *SetEditableMaterialsAtomicCB(RpAtomic *atomic, void *data);
 
+	// SA CVehicleModelInfo::SetupLightFlags (0x4C8C90). Copies the four lamp
+	// flags of the vehicle being drawn into ms_lightsOn, which is the only
+	// thing SetEditableMaterialsCB consults. Called from the vehicle's render
+	// setup, before the clump is drawn.
+	static void SetupLightFlags(CVehicle *vehicle);
+
+	// Resolves ms_pLightsTexture / ms_pLightsOnTexture from vehicle.txd. SA
+	// does this once in SetupCommonData (0x4C75E0, right after the dictionary
+	// itself is loaded); reVC calls it from CCustomModels after the dictionary
+	// has been created, and again whenever the dictionary is rebuilt.
+	static void LoadLightTextures(void);
+	// SA CVehicleModelInfo::ShutdownLightTexture (0x4C7470).
+	static void ShutdownLightTexture(void);
+
+	// The material is one of the two lamp sheets: SA decides by texture
+	// pointer, the name is checked as well so that a car carrying its own copy
+	// of the sheet inside its own TXD is still recognised.
+	static bool IsLightsMaterial(RpMaterial *material);
+
+	// The lit/unlit surface properties of a lamp. SA keeps these in one
+	// RwSurfaceProperties at 0x8A645C and copies it over the material's own
+	// while the lamp is lit; the ambient term is what makes the lit sheet read
+	// as a lamp rather than as a bright sticker under a street light.
+	static const RwSurfaceProperties &GetLightsSurfaceProperties(void);
+
 	// SA CVehicleModelInfo::SetRenderPipelines (0x4C8900). The environment-map
-	// pipeline caches the material's texture, so a remap swap has to re-setup
-	// the pipeline or the car keeps the old skin.
+	// pipeline caches the material's texture, so a remap or lamp swap has to
+	// re-setup the pipeline or the car keeps the old skin.
 	void SetRenderPipelines(void);
 
-	// The materials whose texture this model swapped for a paintjob, and the
-	// texture each of them had. A fixed array rather than a vector: a car has a
-	// bounded number of body materials and this is walked every frame.
+	// One restored material field. SA's tRestoreEntry stores the *address* of
+	// the field and the value it had, so one entry per written field; the enum
+	// below is what this fork stores instead, because the fields a vehicle
+	// material has are few and known, and a tagged entry keeps the list
+	// readable in a debugger.
 	struct RemapEntry
 	{
 		RpMaterial *material;
+		// Texture the material had before the swap (paintjob or lit sheet).
 		RwTexture *texture;
+		// Colour the material had before it was reset to white. Only written
+		// when the colour was changed.
+		RwRGBA colour;
+		// Surface properties the material had before the lit-sheet swap.
+		RwSurfaceProperties surfaceProps;
+		uint8 swappedTexture : 1;
+		uint8 changedColour : 1;
+		uint8 changedSurfaceProps : 1;
 	};
-	enum { NUM_REMAP_ENTRIES = 48 };
+	// SA's gRestoreEntries is 256 entries; a car has one entry per body
+	// material and one per lit lamp, so the same ceiling is kept here.
+	enum { NUM_REMAP_ENTRIES = 256 };
 	RemapEntry m_aRemapEntries[NUM_REMAP_ENTRIES];
 	int32 m_nNumRemapEntries;
 	void SetVehicleColour(uint8 c1, uint8 c2);
diff --git a/src/renderer/Renderer.cpp b/src/renderer/Renderer.cpp
index 5356315..42eaa8a 100644
--- a/src/renderer/Renderer.cpp
+++ b/src/renderer/Renderer.cpp
@@ -316,7 +316,11 @@ CRenderer::RenderOneNonRoad(CEntity *e)
 		CVisibilityPlugins::InitAlphaAtomicList();
 	}
 
-	// Render Peds in vehicle before vehicle itself
+	// SA CRenderer::RenderOneNonRoad (0x553260) brackets the whole draw of a
+	// vehicle with SetupRender and ResetAfterRender: the paintjob, the four
+	// colours and the lamp sheets belong to this car for as long as anything of
+	// it is being drawn, and the sorted alpha atomics below are drawn after
+	// Render has returned, so the swaps have to outlive it.
 	if(e->IsVehicle()){
 		veh = (CVehicle*)e;
 		if(veh->pDriver && veh->pDriver->m_nPedState == PED_DRIVING)
@@ -325,14 +329,17 @@ CRenderer::RenderOneNonRoad(CEntity *e)
 			if(veh->pPassengers[i] && veh->pPassengers[i]->m_nPedState == PED_DRIVING)
 				veh->pPassengers[i]->Render();
 		SetCullMode(rwCULLMODECULLNONE);
+		veh->SetupRender();
 	}
 	e->Render();
 
 	if(e->IsVehicle()){
+		veh = (CVehicle*)e;
 		e->bImBeingRendered = true;
 		CVisibilityPlugins::RenderAlphaAtomics();
 		e->bImBeingRendered = false;
 		SetCullMode(rwCULLMODECULLBACK);
+		veh->ResetAfterRender();
 	}
 
 	e->RemoveLighting(resetLights);
diff --git a/src/renderer/Shadows.cpp b/src/renderer/Shadows.cpp
index a2d423c..3df7aa5 100644
--- a/src/renderer/Shadows.cpp
+++ b/src/renderer/Shadows.cpp
@@ -42,6 +42,7 @@ RwTexture *gpShadowBikeTex;
 RwTexture *gpShadowBaronTex;
 RwTexture *gpShadowExplosionTex;
 RwTexture *gpShadowHeadLightsTex;
+RwTexture *gpShadowHeadLightsTex2;
 RwTexture *gpOutline1Tex;
 RwTexture *gpOutline2Tex;
 RwTexture *gpOutline3Tex;
@@ -78,6 +79,7 @@ CShadows::Init(void)
 	gpShadowBaronTex      = RwTextureRead("shad_rcbaron", nil);
 	gpShadowExplosionTex  = RwTextureRead("shad_exp",     nil);
 	gpShadowHeadLightsTex = RwTextureRead("headlight",    nil);
+	gpShadowHeadLightsTex2 = RwTextureRead("headlight1",   nil);
 	gpOutline1Tex         = RwTextureRead("outline_64",   nil);
 	gpOutline2Tex         = RwTextureRead("outline2_64",  nil);
 	gpOutline3Tex         = RwTextureRead("outline3_64",  nil);
@@ -96,6 +98,10 @@ CShadows::Init(void)
 	ASSERT(gpShadowBaronTex != nil);
 	ASSERT(gpShadowExplosionTex != nil);
 	ASSERT(gpShadowHeadLightsTex != nil);
+	// A Vice City install ships only the one "headlight" texture; the twin pool
+	// then falls back to it instead of failing the whole shadow system.
+	if(gpShadowHeadLightsTex2 == nil && gpShadowHeadLightsTex)
+		gpShadowHeadLightsTex2 = gpShadowHeadLightsTex;
 	ASSERT(gpOutline1Tex != nil);
 	ASSERT(gpOutline2Tex != nil);
 	ASSERT(gpOutline3Tex != nil);
@@ -191,6 +197,8 @@ CShadows::Shutdown(void)
 	RwTextureDestroy(gpShadowBaronTex);
 	RwTextureDestroy(gpShadowExplosionTex);
 	RwTextureDestroy(gpShadowHeadLightsTex);
+	if(gpShadowHeadLightsTex2 && gpShadowHeadLightsTex2 != gpShadowHeadLightsTex)
+		RwTextureDestroy(gpShadowHeadLightsTex2);
 	RwTextureDestroy(gpOutline1Tex);
 	RwTextureDestroy(gpOutline2Tex);
 	RwTextureDestroy(gpOutline3Tex);
diff --git a/src/renderer/Shadows.h b/src/renderer/Shadows.h
index baac830..c0f0ebd 100644
--- a/src/renderer/Shadows.h
+++ b/src/renderer/Shadows.h
@@ -198,6 +198,10 @@ extern RwTexture *gpShadowBikeTex;
 extern RwTexture *gpShadowBaronTex;
 extern RwTexture *gpShadowExplosionTex;
 extern RwTexture *gpShadowHeadLightsTex;
+// SA gpShadowHeadLightsTex2: the second headlight pool texture, the one a
+// vehicle with a single lamp (a bike, the quad) throws on the road;
+// gpShadowHeadLightsTex above is the two-lamp pool of a car.
+extern RwTexture *gpShadowHeadLightsTex2;
 extern RwTexture *gpOutline1Tex;
 extern RwTexture *gpOutline2Tex;
 extern RwTexture *gpOutline3Tex;
diff --git a/src/renderer/SunShadowMap.cpp b/src/renderer/SunShadowMap.cpp
index d653bc6..822059d 100644
--- a/src/renderer/SunShadowMap.cpp
+++ b/src/renderer/SunShadowMap.cpp
@@ -107,11 +107,16 @@ RenderShadowEntity(CEntity *ent)
 			}
 		}
 
+		// The same bracket CRenderer::RenderOneNonRoad puts around a vehicle:
+		// the shadow pass draws the same materials, and a material left
+		// swapped by this pass would be visible in the main one.
+		veh->SetupRender();
 		veh->Render();
 		veh->bImBeingRendered = true;
 		CVisibilityPlugins::RenderAlphaAtomics();
 		CVisibilityPlugins::RenderBoatAlphaAtomics();
 		veh->bImBeingRendered = false;
+		veh->ResetAfterRender();
 		return;
 	}
 
diff --git a/src/rw/TxdStore.cpp b/src/rw/TxdStore.cpp
index b3453a1..fda05b3 100644
--- a/src/rw/TxdStore.cpp
+++ b/src/rw/TxdStore.cpp
@@ -5,6 +5,7 @@
 #include "Streaming.h"
 #include "RwHelper.h"
 #include "TxdStore.h"
+#include "VehicleModelInfo.h"
 #include <string>
 #include <unordered_map>
 #include <set>
@@ -18,6 +19,9 @@ int CTxdStore::ms_currentTxdSlot = -1;
 int CTxdStore::ms_storedTxdSlot = -1;
 int CTxdStore::ms_vehicleTxdSlot = -1;
 static_assert(TXDSTORESIZE <= 32768, "CBaseModelInfo stores TXD slot in signed int16");
+// Defined next to RemoveTxd: drops the lamp sheets CVehicleModelInfo caches when
+// the vehicle dictionary they live in is destroyed.
+static void ForgetCachedLightTextures(int slot);
 
 namespace {
 // The pool may hold tens of thousands of BR TXDs. A linear name lookup or
@@ -131,6 +135,7 @@ CTxdStore::RemoveTxdSlot(int slot)
 		it->second.erase(slot);
 		if(it->second.empty()) txdSlotsByName.erase(it);
 	}
+	ForgetCachedLightTextures(slot);
 	if(def->texDict)
 		RwTexDictionaryDestroy(def->texDict);
 	ms_pTxdPool->Delete(def);
@@ -341,12 +346,27 @@ CTxdStore::FinishLoadTxd(int slot, RwStream *stream)
 	return def->texDict != nil;
 }
 
+// The lamp sheets CVehicleModelInfo caches are textures of the vehicle
+// dictionary. When that dictionary is destroyed or replaced - a game restart, a
+// custom vehicle.txd being swapped in, a BR archive being remounted - the cached
+// pointers would point into freed memory, so they are dropped with it. The
+// model-info code resolves them again the next time the dictionary is loaded.
+static void
+ForgetCachedLightTextures(int slot)
+{
+	if(slot < 0 || slot != CTxdStore::GetVehicleTxdSlot())
+		return;
+	CVehicleModelInfo::ms_pLightsTexture = nil;
+	CVehicleModelInfo::ms_pLightsOnTexture = nil;
+}
+
 void
 CTxdStore::RemoveTxd(int slot)
 {
 #ifdef CUSTOM_MODELS
 	CCustomModels::ForgetTxdSlot(slot);
 #endif
+	ForgetCachedLightTextures(slot);
 	TxdDef *def = GetSlot(slot);
 	if(def->texDict)
 		RwTexDictionaryDestroy(def->texDict);
diff --git a/src/vehicles/Automobile.cpp b/src/vehicles/Automobile.cpp
index b6b95d9..021dbb7 100644
--- a/src/vehicles/Automobile.cpp
+++ b/src/vehicles/Automobile.cpp
@@ -2235,358 +2235,15 @@ CAutomobile::PreRender(void)
 
 	if(GetModelIndex() != MI_RCBANDIT && GetModelIndex() != MI_DODO &&
 	   GetModelIndex() != MI_RHINO && GetModelIndex() != MI_RCBARON &&
-	   GetVehicleAppearance() != VEHICLE_APPEARANCE_HELI) {
-	// Process lights
-
-	// Turn lights on/off
-	bool shouldLightsBeOn = 
-		CClock::GetHours() > 20 ||
-		CClock::GetHours() > 19 && CClock::GetMinutes() > (m_randomSeed & 0x3F) ||
-		CClock::GetHours() < 7 ||
-		CClock::GetHours() < 8 && CClock::GetMinutes() < (m_randomSeed & 0x3F) ||
-		m_randomSeed/50000.0f < CWeather::Foggyness ||
-		m_randomSeed/50000.0f < CWeather::WetRoads;
-	if(shouldLightsBeOn != bLightsOn && GetStatus() != STATUS_WRECKED){
-		if(GetStatus() == STATUS_ABANDONED){
-			// Turn off lights on abandoned vehicles only when we they're far away
-			if(bLightsOn &&
-			   Abs(TheCamera.GetPosition().x - GetPosition().x) + Abs(TheCamera.GetPosition().y - GetPosition().y) > 100.0f)
-				bLightsOn = false;
-		}else
-			bLightsOn = shouldLightsBeOn;
-	}
-
-	// Actually render the lights
-	bool alarmOn = false;
-	bool alarmOff = false;
-	if(IsAlarmOn()){
-		if(CTimer::GetTimeInMilliseconds() & 0x100)
-			alarmOn = true;
-		else
-			alarmOff = true;
-	}
-	if(bEngineOn && bLightsOn || alarmOn || alarmOff){
-		CVector lookVector = GetPosition() - TheCamera.GetPosition();
-		float camDist = lookVector.Magnitude();
-		if(camDist != 0.0f)
-			lookVector *= 1.0f/camDist;
-		else
-			lookVector = CVector(1.0f, 0.0f, 0.0f);
-
-		// 1.0 if directly behind car, -1.0 if in front
-		float behindness = DotProduct(lookVector, GetForward());
-		behindness = Clamp(behindness, -1.0f, 1.0f);	// shouldn't be necessary
-		// 0.0 if behind car, PI if in front
-		// Abs not necessary
-		float angle = Abs(Acos(behindness));
-
-		// Headlights
-		const float vehLightMult = Max(TheCamera.LODDistMultiplier, CRenderer::ms_fLightDistScale);
-		// SA CVehicle::RenderLights resets this each pass and sets a bit as each
-		// lamp registers a corona.
-		m_nRenderLightsFlags = 0;
-
-		CVector headLightPos = mi->m_positions[CAR_POS_HEADLIGHTS];
-		CVector lightR = GetMatrix() * headLightPos;
-		CVector lightL = lightR;
-		lightL -= GetRight()*2.0f*headLightPos.x;
-
-		// Headlight coronas
-		//
-		// SA CVehicle::DoLightEffectImpl (0x6E1780) derives both intensity and
-		// size from normAngle = sqrt(cos angle), not from the cosine itself:
-		//   intensity = normAngle * 0.5f + 0.3f
-		//   size      = (1 - distToCam/150) * normAngle * FRONT_SIZE_ROT_MULT(0.4)
-		// The VC build uses -0.5*behindness + 0.3 and (1 - behindness) instead, so
-		// the falloff is linear rather than square-rooted and the size is far too
-		// large near the car. That is why the lens glow does not look like SA.
-		const float normAngle = Sqrt(angle);
-		if(DotProduct(lightR-TheCamera.GetPosition(), GetForward()) < 0.0f &&
-		   (TheCamera.Cams[TheCamera.ActiveCam].Mode != CCam::MODE_1STPERSON || this != FindPlayerVehicle())){
-			// In front of car
-			float intensity = normAngle*0.5f + 0.3f;
-			float size = (1.0f - camDist*(1.0f/150.0f)) * normAngle * 0.4f;
-
-			if(behindness < -0.97f && camDist < 40.0f * Max(1.0f, CRenderer::ms_fLightDistScale)){
-				// Directly in front and not too far away
-				if(m_pHandlingData->m_nHandlingFlags & VEHICLE_HANDLING_HALOGEN_LIGHTS){
-					if(Damage.GetLightStatus(VEHLIGHT_FRONT_LEFT) == LIGHT_STATUS_OK)
-						CCoronas::RegisterCorona((uintptr)this + 6, 150, 150, 195, 255,
-							lightL, 0.075f, 150.0f*vehLightMult,
-							CCoronas::TYPE_HEADLIGHT, CCoronas::FLARE_NONE, CCoronas::REFLECTION_SIMPLE,
-							CCoronas::LOSCHECK_OFF, CCoronas::STREAK_OFF, normAngle, false, 0.3f);
-					if(Damage.GetLightStatus(VEHLIGHT_FRONT_RIGHT) == LIGHT_STATUS_OK)
-						CCoronas::RegisterCorona((uintptr)this + 7, 150, 150, 195, 255,
-							lightR, 0.075f, 150.0f*vehLightMult,
-							CCoronas::TYPE_HEADLIGHT, CCoronas::FLARE_NONE, CCoronas::REFLECTION_SIMPLE,
-							CCoronas::LOSCHECK_OFF, CCoronas::STREAK_OFF, normAngle, false, 0.3f);
-				}else{
-					if(Damage.GetLightStatus(VEHLIGHT_FRONT_LEFT) == LIGHT_STATUS_OK)
-						CCoronas::RegisterCorona((uintptr)this + 6, 160, 160, 140, 255,
-							lightL, 0.075f, 150.0f*vehLightMult,
-							CCoronas::TYPE_HEADLIGHT, CCoronas::FLARE_NONE, CCoronas::REFLECTION_SIMPLE,
-							CCoronas::LOSCHECK_OFF, CCoronas::STREAK_OFF, normAngle, false, 0.3f);
-					if(Damage.GetLightStatus(VEHLIGHT_FRONT_RIGHT) == LIGHT_STATUS_OK)
-						CCoronas::RegisterCorona((uintptr)this + 7, 160, 160, 140, 255,
-							lightR, 0.075f, 150.0f*vehLightMult,
-							CCoronas::TYPE_HEADLIGHT, CCoronas::FLARE_NONE, CCoronas::REFLECTION_SIMPLE,
-							CCoronas::LOSCHECK_OFF, CCoronas::STREAK_OFF, normAngle, false, 0.3f);
-				}
-			}
-
-			if(alarmOff){
-				if(Damage.GetLightStatus(VEHLIGHT_FRONT_LEFT) == LIGHT_STATUS_OK)
-					CCoronas::RegisterCorona((uintptr)this, 0, 0, 0, 0,
-						lightL, size, 0.0f,
-						CCoronas::TYPE_STREAK, CCoronas::FLARE_NONE, CCoronas::REFLECTION_ON,
-						CCoronas::LOSCHECK_OFF, CCoronas::STREAK_ON, angle);
-				if(Damage.GetLightStatus(VEHLIGHT_FRONT_RIGHT) == LIGHT_STATUS_OK)
-					CCoronas::RegisterCorona((uintptr)this + 1, 0, 0, 0, 0,
-						lightR, size, 0.0f,
-						CCoronas::TYPE_STREAK, CCoronas::FLARE_NONE, CCoronas::REFLECTION_ON,
-						CCoronas::LOSCHECK_OFF, CCoronas::STREAK_ON, angle);
-			}else{
-				// SA CVehicle::DoLightEffectImpl, headlight branch, verbatim:
-				//   halogen: r = 190*intensity, g = intensity*255
-				//   normal : r = 210*intensity, g = intensity*195
-				//   then RegisterCorona(..., r, r, g, 128, ...)
-				// The VC build registers (r, r, b) instead of (r, r, g) and passes
-				// alpha 255. Two consequences: the lens comes out blue because the
-				// third channel is the *blue* multiplier, and the cornea is twice as
-				// opaque as SA's. This is the "one lens blue, one yellow" looking
-				// wrong even when both are computed identically.
-				uint8 lightColorR, lightColorG;
-				if(m_pHandlingData->m_nHandlingFlags & VEHICLE_HANDLING_HALOGEN_LIGHTS){
-					lightColorR = (uint8)(190.0f*intensity);
-					lightColorG = (uint8)(intensity*255.0f);
-				}else{
-					lightColorR = (uint8)(210.0f*intensity);
-					lightColorG = (uint8)(intensity*195.0f);
-				}
-				// SA registers one corona per side through this lambda, with the
-				// colour triple being (lightColorR, lightColorR, lightColorG).
-				if(Damage.GetLightStatus(VEHLIGHT_FRONT_LEFT) == LIGHT_STATUS_OK)
-					CCoronas::RegisterCorona((uintptr)this + 2*(int32)0 + 0, lightColorR, lightColorR, lightColorG, 128,
-						lightL, size, 150.0f*vehLightMult,
-						CCoronas::TYPE_HEADLIGHT, CCoronas::FLARE_NONE, CCoronas::REFLECTION_SIMPLE,
-						CCoronas::LOSCHECK_OFF, CCoronas::STREAK_OFF, normAngle,
-						false, 0.5f);
-				if(Damage.GetLightStatus(VEHLIGHT_FRONT_RIGHT) == LIGHT_STATUS_OK){
-					CCoronas::RegisterCorona((uintptr)this + 2*(int32)0 + 1, lightColorR, lightColorR, lightColorG, 128,
-						lightR, size, 150.0f*vehLightMult,
-						CCoronas::TYPE_HEADLIGHT, CCoronas::FLARE_NONE, CCoronas::REFLECTION_SIMPLE,
-						CCoronas::LOSCHECK_OFF, CCoronas::STREAK_OFF, normAngle,
-						false, 0.5f);
-					m_renderLights.m_bRightFront = 1;
-				}
-
-				// SA RenderLights() repeats both calls for eVehicleLightId::SECONDARY,
-				// i.e. the "headlights2" dummy. Models without that dummy leave the
-				// position at zero and the second pass registers nothing (SA returns
-				// early on a zero dummy), so cars with two lamp quads get four lit
-				// lenses and cars with one are unchanged.
-				CVector secondaryPos = mi->m_vehicleStruct.m_avDummyPos[DUMMY_LIGHT_FRONT_SECONDARY];
-				if(secondaryPos.x != 0.0f || secondaryPos.y != 0.0f || secondaryPos.z != 0.0f){
-					CVector lightR2 = GetMatrix() * secondaryPos;
-					CVector lightL2 = lightR2;
-					lightL2 -= GetRight()*2.0f*secondaryPos.x;
-
-					if(Damage.GetLightStatus(VEHLIGHT_FRONT_LEFT) == LIGHT_STATUS_OK)
-						CCoronas::RegisterCorona((uintptr)this + 2*(int32)1 + 0, lightColorR, lightColorR, lightColorG, 128,
-							lightL2, size, 150.0f*vehLightMult,
-							CCoronas::TYPE_HEADLIGHT, CCoronas::FLARE_NONE, CCoronas::REFLECTION_SIMPLE,
-							CCoronas::LOSCHECK_OFF, CCoronas::STREAK_OFF, normAngle, false, 0.5f);
-					if(Damage.GetLightStatus(VEHLIGHT_FRONT_RIGHT) == LIGHT_STATUS_OK)
-						CCoronas::RegisterCorona((uintptr)this + 2*(int32)1 + 1, lightColorR, lightColorR, lightColorG, 128,
-							lightR2, size, 150.0f*vehLightMult,
-							CCoronas::TYPE_HEADLIGHT, CCoronas::FLARE_NONE, CCoronas::REFLECTION_SIMPLE,
-							CCoronas::LOSCHECK_OFF, CCoronas::STREAK_OFF, normAngle, false, 0.5f);
-				}
-			}
-		}else{
-			// Behind car
-			if(Damage.GetLightStatus(VEHLIGHT_FRONT_LEFT) == LIGHT_STATUS_OK)
-				CCoronas::UpdateCoronaCoors((uintptr)this, lightL, 80.0f*vehLightMult, angle);
-			if(Damage.GetLightStatus(VEHLIGHT_FRONT_RIGHT) == LIGHT_STATUS_OK)
-				CCoronas::UpdateCoronaCoors((uintptr)this + 1, lightR, 80.0f*vehLightMult, angle);
-		}
-
-		// SA has no bright-light cubes on cars. CBrightLights there only ever
-		// draws traffic lights (its sole caller is CTrafficLights), and a car's
-		// headlight is the corona registered above plus the point light in
-		// CVehicle::DoVehicleLights. The cubes VC registered here were a
-		// different, box-shaped effect standing in for the lens, which is why
-		// they read as "boxes" rather than light. Removed to match SA.
-
-		// Taillights
-
-		CVector tailLightPos = mi->m_positions[CAR_POS_TAILLIGHTS];
-		lightR = GetMatrix() * tailLightPos;
-		lightL = lightR;
-		lightL -= GetRight()*2.0f*tailLightPos.x;
-
-		// Taillight coronas
-		//
-		// SA CVehicle::DoLightEffectImpl with isFront=false:
-		//   size      = (1 - distToCam/150) * angle * REAR_SIZE_ROT_MULT(0.2)
-		//   intensity = angle * 0.5f + 0.2f        (angle, not sqrt, for tail)
-		//   brake     -> RegisterCorona(..., 128*intensity, 0, 0, 128, ...)
-		//   emission  -> RegisterCorona(..., 96*intensity, 0, 0, 128, ...)
-		// VC used (behindness + 1) * 0.5 for size and 255 alpha, and lit both
-		// sides white while reversing.
-		if(DotProduct(lightR-TheCamera.GetPosition(), GetForward()) > 0.0f){
-			// Behind car
-			float intensity = angle*0.5f + 0.2f;
-			float size = (1.0f - camDist*(1.0f/150.0f)) * angle * 0.2f;
-
-			if(m_fGasPedal < 0.0f){
-				// reversing
-				intensity += 0.4f;
-				size += 0.3f;
-				if(Damage.GetLightStatus(VEHLIGHT_REAR_LEFT) == LIGHT_STATUS_OK)
-					CCoronas::RegisterCorona((uintptr)this + 14, (uint8)(128*intensity), 0, 0, 128,
-						lightL, size, 150.0f*vehLightMult,
-						CCoronas::TYPE_HEADLIGHT, CCoronas::FLARE_NONE, CCoronas::REFLECTION_SIMPLE,
-						CCoronas::LOSCHECK_OFF, CCoronas::STREAK_OFF, angle);
-				if(Damage.GetLightStatus(VEHLIGHT_REAR_RIGHT) == LIGHT_STATUS_OK)
-					CCoronas::RegisterCorona((uintptr)this + 15, (uint8)(128*intensity), 0, 0, 128,
-						lightR, size, 150.0f*vehLightMult,
-						CCoronas::TYPE_HEADLIGHT, CCoronas::FLARE_NONE, CCoronas::REFLECTION_SIMPLE,
-						CCoronas::LOSCHECK_OFF, CCoronas::STREAK_OFF, angle);
-			}else{
-				if(m_fBrakePedal > 0.0f){
-					intensity += 0.4f;
-					size += 0.3f;
-				}
-
-				if(alarmOff){
-					if(Damage.GetLightStatus(VEHLIGHT_REAR_LEFT) == LIGHT_STATUS_OK)
-						CCoronas::RegisterCorona((uintptr)this + 14, 0, 0, 0, 0,
-							lightL, size, 0.0f,
-							CCoronas::TYPE_STREAK, CCoronas::FLARE_NONE, CCoronas::REFLECTION_ON,
-							CCoronas::LOSCHECK_OFF, CCoronas::STREAK_ON, angle);
-					if(Damage.GetLightStatus(VEHLIGHT_REAR_RIGHT) == LIGHT_STATUS_OK)
-						CCoronas::RegisterCorona((uintptr)this + 15, 0, 0, 0, 0,
-							lightR, size, 0.0f,
-							CCoronas::TYPE_STREAK, CCoronas::FLARE_NONE, CCoronas::REFLECTION_ON,
-							CCoronas::LOSCHECK_OFF, CCoronas::STREAK_ON, angle);
-				}else{
-					if(Damage.GetLightStatus(VEHLIGHT_REAR_LEFT) == LIGHT_STATUS_OK)
-						CCoronas::RegisterCorona((uintptr)this + 14, (uint8)(128*intensity), 0, 0, 128,
-							lightL, size, 150.0f*vehLightMult,
-							CCoronas::TYPE_HEADLIGHT, CCoronas::FLARE_NONE, CCoronas::REFLECTION_SIMPLE,
-							CCoronas::LOSCHECK_OFF, CCoronas::STREAK_OFF, angle);
-					if(Damage.GetLightStatus(VEHLIGHT_REAR_RIGHT) == LIGHT_STATUS_OK)
-						CCoronas::RegisterCorona((uintptr)this + 15, (uint8)(128*intensity), 0, 0, 128,
-							lightR, size, 150.0f*vehLightMult,
-							CCoronas::TYPE_HEADLIGHT, CCoronas::FLARE_NONE, CCoronas::REFLECTION_SIMPLE,
-							CCoronas::LOSCHECK_OFF, CCoronas::STREAK_OFF, angle);
-				}
-			}
-		}else{
-			// In front of car
-			if(Damage.GetLightStatus(VEHLIGHT_REAR_LEFT) == LIGHT_STATUS_OK)
-				CCoronas::UpdateCoronaCoors((uintptr)this + 14, lightL, 80.0f*vehLightMult, angle);
-			if(Damage.GetLightStatus(VEHLIGHT_REAR_RIGHT) == LIGHT_STATUS_OK)
-				CCoronas::UpdateCoronaCoors((uintptr)this + 15, lightR, 80.0f*vehLightMult, angle);
-		}
-
-		// SA registers no bright-light geometry for tail lights either; the
-		// corona above is the whole effect.
-
-		// Light shadows
-		if(!alarmOff){
-			CVector pos = GetPosition();
-			CVector2D fwd(GetForward());
-			fwd.Normalise();
-			float f = headLightPos.y + 6.0f;
-			pos += CVector(f*fwd.x, f*fwd.y, 2.0f);
-
-			if(Damage.GetLightStatus(VEHLIGHT_FRONT_LEFT) == LIGHT_STATUS_OK ||
-			   Damage.GetLightStatus(VEHLIGHT_FRONT_RIGHT) == LIGHT_STATUS_OK)
-				CShadows::StoreCarLightShadow(this, (uintptr)this + 22, gpShadowHeadLightsTex, &pos,
-					7.0f*fwd.x, 7.0f*fwd.y, 5.5f*fwd.y, -5.5f*fwd.x, 45, 45, 45, 7.0f);
-
-			f = (tailLightPos.y - 2.5f) - (headLightPos.y + 6.0f);
-			pos += CVector(f*fwd.x, f*fwd.y, 0.0f);
-			if(Damage.GetLightStatus(VEHLIGHT_REAR_LEFT) == LIGHT_STATUS_OK ||
-			   Damage.GetLightStatus(VEHLIGHT_REAR_RIGHT) == LIGHT_STATUS_OK)
-				CShadows::StoreCarLightShadow(this, (uintptr)this + 25, gpShadowExplosionTex, &pos,
-					3.0f, 0.0f, 0.0f, -3.0f, 35, 0, 0, 4.0f);
-		}
-
-		// SA CVehicle::DoVehicleLights: the directional headlight light is an
-		// effect of every lit car, not only the player's, and the brake light at
-		// the rear is added only when the driver is actually on the brakes with
-		// the handbrake off. Ranges and colours are the SA constants (20 / 1,1,1
-		// for the headlights, 10 with 0.1/0.02/0.02 for the brakes).
-		if(Damage.GetLightStatus(VEHLIGHT_FRONT_LEFT) == LIGHT_STATUS_OK ||
-		   Damage.GetLightStatus(VEHLIGHT_FRONT_RIGHT) == LIGHT_STATUS_OK)
-			CPointLights::AddLight(CPointLights::LIGHT_DIRECTIONAL, GetMatrix().GetPosition(), GetForward(),
-				20.0f, 1.0f, 1.0f, 1.0f,
-				m_vecMoveSpeed.MagnitudeSqr2D() < sq(0.45f) ? CPointLights::FOG_NORMAL : CPointLights::FOG_NONE,
-				false);
-
-		if((Damage.GetLightStatus(VEHLIGHT_REAR_LEFT) == LIGHT_STATUS_OK ||
-		    Damage.GetLightStatus(VEHLIGHT_REAR_RIGHT) == LIGHT_STATUS_OK) &&
-		   m_fBrakePedal > 0.0f && !bIsHandbrakeOn && pDriver){
-			CPointLights::AddLight(CPointLights::LIGHT_DIRECTIONAL,
-				GetPosition() + -4.0f*GetForward(), -GetForward(),
-				10.0f, 0.1f, 0.02f, 0.02f,
-				CPointLights::FOG_NONE, false);
-		}
-	}else if(GetStatus() != STATUS_ABANDONED && GetStatus() != STATUS_WRECKED){
-		// Lights off
-
-		CVector lightPos = mi->m_positions[CAR_POS_TAILLIGHTS];
-		CVector lightR = GetMatrix() * lightPos;
-		CVector lightL = lightR;
-		lightL -= GetRight()*2.0f*lightPos.x;
-
-		if(m_fBrakePedal > 0.0f || m_fGasPedal < 0.0f){
-			CVector lookVector = GetPosition() - TheCamera.GetPosition();
-			lookVector.Normalise();
-			float behindness = DotProduct(lookVector, GetForward());
-			if(behindness > 0.0f){
-				if(m_fGasPedal < 0.0f){
-					// reversing
-					if(Damage.GetLightStatus(VEHLIGHT_REAR_LEFT) == LIGHT_STATUS_OK)
-						CCoronas::RegisterCorona((uintptr)this + 14, 120, 120, 120, 255,
-							lightL, 1.2f, 50.0f*TheCamera.LODDistMultiplier,
-							CCoronas::TYPE_STAR, CCoronas::FLARE_NONE, CCoronas::REFLECTION_ON,
-							CCoronas::LOSCHECK_OFF, CCoronas::STREAK_ON, 0.0f);
-					if(Damage.GetLightStatus(VEHLIGHT_REAR_RIGHT) == LIGHT_STATUS_OK)
-						CCoronas::RegisterCorona((uintptr)this + 15, 120, 120, 120, 255,
-							lightR, 1.2f, 50.0f*TheCamera.LODDistMultiplier,
-							CCoronas::TYPE_STAR, CCoronas::FLARE_NONE, CCoronas::REFLECTION_ON,
-							CCoronas::LOSCHECK_OFF, CCoronas::STREAK_ON, 0.0f);
-				}else{
-					// braking
-					if(Damage.GetLightStatus(VEHLIGHT_REAR_LEFT) == LIGHT_STATUS_OK)
-						CCoronas::RegisterCorona((uintptr)this + 14, 120, 0, 0, 255,
-							lightL, 1.2f, 50.0f*TheCamera.LODDistMultiplier,
-							CCoronas::TYPE_STAR, CCoronas::FLARE_NONE, CCoronas::REFLECTION_ON,
-							CCoronas::LOSCHECK_OFF, CCoronas::STREAK_ON, 0.0f);
-					if(Damage.GetLightStatus(VEHLIGHT_REAR_RIGHT) == LIGHT_STATUS_OK)
-						CCoronas::RegisterCorona((uintptr)this + 15, 120, 0, 0, 255,
-							lightR, 1.2f, 50.0f*TheCamera.LODDistMultiplier,
-							CCoronas::TYPE_STAR, CCoronas::FLARE_NONE, CCoronas::REFLECTION_ON,
-							CCoronas::LOSCHECK_OFF, CCoronas::STREAK_ON, 0.0f);
-				}
-			}else{
-				if(Damage.GetLightStatus(VEHLIGHT_REAR_LEFT) == LIGHT_STATUS_OK)
-					CCoronas::UpdateCoronaCoors((uintptr)this + 14, lightL, 50.0f*TheCamera.LODDistMultiplier, 0.0f);
-				if(Damage.GetLightStatus(VEHLIGHT_REAR_RIGHT) == LIGHT_STATUS_OK)
-					CCoronas::UpdateCoronaCoors((uintptr)this + 15, lightR, 50.0f*TheCamera.LODDistMultiplier, 0.0f);
-			}
-		}else{
-			if(Damage.GetLightStatus(VEHLIGHT_REAR_LEFT) == LIGHT_STATUS_OK)
-				CCoronas::UpdateCoronaCoors((uintptr)this + 14, lightL, 50.0f*TheCamera.LODDistMultiplier, 0.0f);
-			if(Damage.GetLightStatus(VEHLIGHT_REAR_RIGHT) == LIGHT_STATUS_OK)
-				CCoronas::UpdateCoronaCoors((uintptr)this + 15, lightR, 50.0f*TheCamera.LODDistMultiplier, 0.0f);
-		}
-	}
-	// end of lights
+	   GetVehicleAppearance() != VEHICLE_APPEARANCE_HELI){
+		// SA CVehicle::DoVehicleLights (0x6E1A60). Everything the lamps do -
+		// the time-of-day decision, the four lamp materials, the coronas, the
+		// light shadows, the point lights and the flags the render setup
+		// reads - happens in that one function. The VC build split it between
+		// two blocks here that disagreed with each other about colour, size,
+		// damage and reversing lights, and filled none of m_renderLights, so
+		// the lamp sheets never lit and CBike::Render never drew a beam.
+		DoVehicleLights(GetMatrix(), GetVehicleLightsFlags());
 	}
 
 	if (IsRealHeli())
@@ -2969,20 +2626,13 @@ CAutomobile::PreRender(void)
 void
 CAutomobile::Render(void)
 {
-	CVehicleModelInfo *mi = (CVehicleModelInfo*)CModelInfo::GetModelInfo(GetModelIndex());
-
-	// The car's paintjob goes onto the model's editable materials for this
-	// frame only: ApplyRemapTexture publishes the texture the model's material
-	// path swaps in, and the model restores the material's own texture after the
-	// clump has been drawn. The four colours are applied together because SA
-	// paints four material lists, not two.
-	ApplyRemapTexture();
-	mi->SetVehicleColour(m_currentColour1, m_currentColour2, m_currentColour3, m_currentColour4);
-	// Swap the body texture for the paintjob. The model is shared by every car
-	// of this model, so this is bracketed: the materials are put back right
-	// after the clump has been drawn, at the end of this function.
-	if(m_pRemapTexture)
-		mi->SetEditableMaterials();
+	// The colours, the paintjob and the lamp sheets are this car's for the
+	// duration of this draw. They are set up by CVehicle::SetupRender, which
+	// CRenderer calls before Render because the sorted alpha pass draws atomics
+	// of this vehicle after Render has returned; they are put back by
+	// CVehicle::ResetAfterRender right after that pass. Render itself must not
+	// touch them, or the alpha atomics would be drawn with the model's own
+	// textures while the rest of the car wore the swapped ones.
 
 	if(IsRealHeli()){
 		RpAtomic *atomic = nil;
@@ -3032,10 +2682,9 @@ CAutomobile::Render(void)
 	}else
 		CEntity::Render();
 
-	// The clump is drawn; give the shared model its own textures back so the
-	// next car of the same model does not inherit this one's paintjob.
-	if(m_pRemapTexture)
-		mi->RestoreEditableMaterials();
+	// The clump is drawn. The model's own textures are put back by
+	// CVehicle::ResetAfterRender, after the alpha pass has drawn the rest of
+	// this vehicle.
 }
 
 int32
diff --git a/src/vehicles/Automobile.h b/src/vehicles/Automobile.h
index 19b6e91..f08db62 100644
--- a/src/vehicles/Automobile.h
+++ b/src/vehicles/Automobile.h
@@ -201,8 +201,12 @@ public:
 	// functions the collisions call rather than a shortcut that only moves the
 	// bits.
 
-	CDamageManager &GetDamageManager(void) { return Damage; }
-	const CDamageManager &GetDamageManager(void) const { return Damage; }
+	virtual CDamageManager *GetDamageManager(void) { return &Damage; }
+	virtual const CDamageManager *GetDamageManager(void) const { return &Damage; }
+	// The typed reference is what the car's own code uses; the pair above is
+	// the CVehicle interface the lamp pass goes through.
+	CDamageManager &GetDamage(void) { return Damage; }
+	const CDamageManager &GetDamage(void) const { return Damage; }
 
 	// Per-part setters. Each takes the part's own status, so a peer sends the
 	// same values GetPartStatus reports back.
diff --git a/src/vehicles/Bike.cpp b/src/vehicles/Bike.cpp
index da175db..b978557 100644
--- a/src/vehicles/Bike.cpp
+++ b/src/vehicles/Bike.cpp
@@ -1336,219 +1336,20 @@ CBike::PreRender(void)
 	}
 
 	// Process lights
-
-	// Turn lights on/off
-	bool shouldLightsBeOn = 
-		CClock::GetHours() > 20 ||
-		CClock::GetHours() > 19 && CClock::GetMinutes() > (m_randomSeed & 0x3F) ||
-		CClock::GetHours() < 7 ||
-		CClock::GetHours() < 8 && CClock::GetMinutes() < (m_randomSeed & 0x3F) ||
-		m_randomSeed/50000.0f < CWeather::Foggyness ||
-		m_randomSeed/50000.0f < CWeather::WetRoads;
-	if(shouldLightsBeOn != bLightsOn && GetStatus() != STATUS_WRECKED){
-		if(GetStatus() == STATUS_ABANDONED){
-			// Turn off lights on abandoned vehicles only when we they're far away
-			if(bLightsOn &&
-			   Abs(TheCamera.GetPosition().x - GetPosition().x) + Abs(TheCamera.GetPosition().y - GetPosition().y) > 100.0f)
-				bLightsOn = false;
-		}else
-			bLightsOn = shouldLightsBeOn;
-	}
-
-	// Actually render the lights
-	bool alarmOn = false;
-	bool alarmOff = false;
-	if(IsAlarmOn()){
-		if(CTimer::GetTimeInMilliseconds() & 0x100)
-			alarmOn = true;
-		else
-			alarmOff = true;
-	}
-	if(bEngineOn && bLightsOn || alarmOn || alarmOff){
-		CalculateLeanMatrix();
-		CVector lookVector = GetPosition() - TheCamera.GetPosition();
-		float camDist = lookVector.Magnitude();
-		if(camDist != 0.0f)
-			lookVector *= 1.0f/camDist;
-		else
-			lookVector = CVector(1.0f, 0.0f, 0.0f);
-
-		// 1.0 if directly behind car, -1.0 if in front
-		float behindness = DotProduct(lookVector, GetForward());
-		// 0.0 if behind car, PI if in front
-		float angle = Abs(Acos(Abs(behindness)));
-
-		// Headlight
-
-		CMatrix mat;
-		CVector headLightPos = mi->m_positions[CAR_POS_HEADLIGHTS];
-		if(GetModelIndex() == 152){	// this is the bobcat in VC, but we don't want that effect anyway
-			mat.SetUnity();
-			mat.RotateZ(m_fWheelAngle);
-			mat = m_leanMatrix * mat;
-		}else
-			mat = m_leanMatrix;
-		const float bikeLightMult = Max(TheCamera.LODDistMultiplier, CRenderer::ms_fLightDistScale);
-		// SA RenderLights() sets this as each light registers; CBike::Render
-		// consumes it for the headlight cone.
-		m_nRenderLightsFlags = 0;
-		CVector light = mat * headLightPos;
-		// SA CVehicle::DoLightEffectImpl (0x6E1780) drives both intensity and size
-		// from normAngle = sqrt(cos angle):
-		//   intensity = normAngle * 0.5f + 0.3f
-		//   size      = (1 - distToCam/150) * normAngle * FRONT_SIZE_ROT_MULT(0.4)
-		// VC uses the raw cosine and a linear size, hence the different falloff.
-		const float normAngle = Sqrt(angle);
-		if(behindness < 0.0f){
-			// In front of bike
-			float intensity = normAngle*0.5f + 0.3f;
-			float size = (1.0f - camDist*(1.0f/150.0f)) * normAngle * 0.4f;
-
-			if(behindness < -0.97f && camDist < 40.0f * Max(1.0f, CRenderer::ms_fLightDistScale)){
-				// Directly in front and not too far away
-				if(m_pHandlingData->m_nHandlingFlags & VEHICLE_HANDLING_HALOGEN_LIGHTS){
-					CCoronas::RegisterCorona((uintptr)this + 6, 150, 150, 195, 255,
-						light, 0.075f, 150.0f*bikeLightMult,
-						CCoronas::TYPE_HEADLIGHT, CCoronas::FLARE_NONE, CCoronas::REFLECTION_SIMPLE,
-						CCoronas::LOSCHECK_OFF, CCoronas::STREAK_OFF, normAngle, false, 0.3f);
-				}else{
-					CCoronas::RegisterCorona((uintptr)this + 6, 160, 160, 140, 255,
-						light, 0.075f, 150.0f*bikeLightMult,
-						CCoronas::TYPE_HEADLIGHT, CCoronas::FLARE_NONE, CCoronas::REFLECTION_SIMPLE,
-						CCoronas::LOSCHECK_OFF, CCoronas::STREAK_OFF, normAngle, false, 0.3f);
-				}
-			}
-
-			if(alarmOff){
-				CCoronas::RegisterCorona((uintptr)this, 0, 0, 0, 0,
-					light, size, 0.0f,
-					CCoronas::TYPE_STREAK, CCoronas::FLARE_NONE, CCoronas::REFLECTION_ON,
-					CCoronas::LOSCHECK_OFF, CCoronas::STREAK_ON, angle);
-			}else{
-				// SA: colour triple is (lightColorR, lightColorR, lightColorG) with
-				// alpha 128. VC passes (r, r, b) with alpha 255 - the third channel
-				// is the *blue* multiplier, which tints the lens.
-				uint8 lightColorR, lightColorG;
-				if(m_pHandlingData->m_nHandlingFlags & VEHICLE_HANDLING_HALOGEN_LIGHTS){
-					lightColorR = (uint8)(190.0f*intensity);
-					lightColorG = (uint8)(intensity*255.0f);
-				}else{
-					lightColorR = (uint8)(210.0f*intensity);
-					lightColorG = (uint8)(intensity*195.0f);
-				}
-				CCoronas::RegisterCorona((uintptr)this + 1, lightColorR, lightColorR, lightColorG, 128,
-					light, size, 150.0f*bikeLightMult,
-					CCoronas::TYPE_HEADLIGHT, CCoronas::FLARE_NONE, CCoronas::REFLECTION_SIMPLE,
-					CCoronas::LOSCHECK_OFF, CCoronas::STREAK_OFF, normAngle, false, 0.5f);
-				// A bike has a single headlight; SA records it as the right one.
-				m_renderLights.m_bRightFront = 1;
-			}
-		}else{
-			CCoronas::UpdateCoronaCoors((uintptr)this, light, 80.0f*bikeLightMult, angle);
-		}
-
-
-		// SA draws no bright-light geometry on bikes either; the coronas above
-		// are the entire effect.
-
-		// Taillight
-
-		CVector tailLightPos = mi->m_positions[CAR_POS_TAILLIGHTS];
-		light = m_leanMatrix * tailLightPos;
-
-		// Taillight corona
-		if(behindness > 0.0f){
-			// Behind car
-			float intensity = 0.4f*behindness + 0.4f;
-			float size = (behindness + 1.0f)/2.0f;
-
-			if(m_fGasPedal < 0.0f){
-				// reversing
-				// no lights in this case
-			}else{
-				if(m_fBrakePedal > 0.0f){
-					intensity += 0.4f;
-					size += 0.3f;
-				}
-
-				if(alarmOff){
-					CCoronas::RegisterCorona((uintptr)this + 14, 0, 0, 0, 0,
-						light, size, 0.0f,
-						CCoronas::TYPE_STREAK, CCoronas::FLARE_NONE, CCoronas::REFLECTION_ON,
-						CCoronas::LOSCHECK_OFF, CCoronas::STREAK_ON, angle);
-				}else{
-					CCoronas::RegisterCorona((uintptr)this + 14, 128*intensity, 0, 0, 255,
-						light, size, 80.0f*bikeLightMult,
-						CCoronas::TYPE_STREAK, CCoronas::FLARE_NONE, CCoronas::REFLECTION_ON,
-						CCoronas::LOSCHECK_OFF, CCoronas::STREAK_ON, angle);
-				}
-			}
-		}else{
-			CCoronas::UpdateCoronaCoors((uintptr)this + 14, light, 80.0f*bikeLightMult, angle);
-		}
-
-
-		// Light shadows
-		if(!alarmOff){
-			CVector pos = GetPosition();
-			CVector2D fwd(GetForward());
-			fwd.Normalise();
-			float f = headLightPos.y + 6.0f;
-			pos += CVector(f*fwd.x, f*fwd.y, 2.0f);
-			CShadows::StoreCarLightShadow(this, (uintptr)this + 22, gpShadowExplosionTex, &pos,
-				7.0f*fwd.x, 7.0f*fwd.y, 3.5f*fwd.y, -3.5f*fwd.x, 45, 45, 45, 7.0f);
-
-			f = (tailLightPos.y - 2.5f) - (headLightPos.y + 6.0f);
-			pos += CVector(f*fwd.x, f*fwd.y, 0.0f);
-			CShadows::StoreCarLightShadow(this, (uintptr)this + 25, gpShadowExplosionTex, &pos,
-				3.0f, 0.0f, 0.0f, -3.0f, 35, 0, 0, 4.0f);
-		}
-
-		if(this == FindPlayerVehicle() && !alarmOff){
-			CPointLights::AddLight(CPointLights::LIGHT_DIRECTIONAL, GetPosition(), GetForward(),
-				20.0f, 1.0f, 1.0f, 1.0f,
-				FindPlayerVehicle()->m_vecMoveSpeed.MagnitudeSqr2D() < sq(0.45f) ? CPointLights::FOG_NORMAL : CPointLights::FOG_NONE,
-				false);
-			CVector pos = GetPosition() - 4.0f*GetForward();
-			if(m_fBrakePedal > 0.0f)
-				CPointLights::AddLight(CPointLights::LIGHT_POINT, pos, CVector(0.0f, 0.0f, 0.0f),
-					10.0f, 1.0f, 0.0f, 0.0f,
-					CPointLights::FOG_NONE, false);
-			else
-				CPointLights::AddLight(CPointLights::LIGHT_POINT, pos, CVector(0.0f, 0.0f, 0.0f),
-					7.0f, 0.6f, 0.0f, 0.0f,
-					CPointLights::FOG_NONE, false);
-		}
-	}else if(GetStatus() != STATUS_ABANDONED && GetStatus() != STATUS_WRECKED){
-		// Lights off
-		CalculateLeanMatrix();
-
-		CVector tailLightPos = mi->m_positions[CAR_POS_TAILLIGHTS];
-		CVector light = m_leanMatrix * tailLightPos;
-
-		if(m_fBrakePedal > 0.0f || m_fGasPedal < 0.0f){
-			CVector lookVector = GetPosition() - TheCamera.GetPosition();
-			lookVector.Normalise();
-			float behindness = DotProduct(lookVector, GetForward());
-			if(behindness > 0.0f){
-				if(m_fGasPedal < 0.0f){
-					// reversing
-					// no lights in this case
-				}else{
-					// braking
-					CCoronas::RegisterCorona((uintptr)this + 14, 120, 0, 0, 255,
-						light, 1.2f, 50.0f*TheCamera.LODDistMultiplier,
-						CCoronas::TYPE_STAR, CCoronas::FLARE_NONE, CCoronas::REFLECTION_ON,
-						CCoronas::LOSCHECK_OFF, CCoronas::STREAK_ON, 0.0f);
-				}
-			}else{
-				CCoronas::UpdateCoronaCoors((uintptr)this + 14, light, 50.0f*TheCamera.LODDistMultiplier, 0.0f);
-			}
-		}else{
-			CCoronas::UpdateCoronaCoors((uintptr)this + 14, light, 50.0f*TheCamera.LODDistMultiplier, 0.0f);
-		}
-	}
-
+	//
+	// SA CBike::PreRender (0x6BD090) runs the same CVehicle::DoVehicleLights a
+	// car does, with the bike's own matrix, so a bike's lamps obey the time of
+	// day, the weather, the circuit of the alarm and the script override in
+	// exactly the same way, and m_renderLights is filled the same way, which is
+	// what CBike::Render reads to decide on the headlight cone. The VC code
+	// this replaces kept a second copy of the light logic with its own colours,
+	// its own damage rules and no reversing lights at all.
+	//
+	// The lean matrix is what a bike's lamps hang off: the whole fork tilts with
+	// the bike, so a corona placed from the upright matrix would float beside
+	// the headlight in a corner.
+	CalculateLeanMatrix();
+	DoVehicleLights(m_leanMatrix, GetVehicleLightsFlags());
 
 	// Wheel particles
 
@@ -1763,25 +1564,19 @@ CBike::PreRender(void)
 void
 CBike::Render(void)
 {
-	CVehicleModelInfo *mi = (CVehicleModelInfo*)CModelInfo::GetModelInfo(GetModelIndex());
-
 	m_nSetPieceExtendedRangeTime = CTimer::GetTimeInMilliseconds() + 3000;
-	// A bike wears a paintjob and four colours the same way a car does.
-	ApplyRemapTexture();
-	mi->SetVehicleColour(m_currentColour1, m_currentColour2, m_currentColour3, m_currentColour4);
-	if(m_pRemapTexture)
-		mi->SetEditableMaterials();
+	// A bike wears a paintjob, four colours and lamp sheets the same way a car
+	// does, and CVehicle::SetupRender/ResetAfterRender bracket the draw with
+	// them exactly as for a car. Render must not touch them.
 	CEntity::Render();
-	if(m_pRemapTexture)
-		mi->RestoreEditableMaterials();
 
 	// SA CBike::Render (0x6BDE20): the headlight cone hangs off the *lean*
 	// matrix, so it tilts with the bike. This is the only place in SA that calls
 	// DoHeadLightBeam - cars never draw a beam, which is why adding it there
 	// would not be a port.
-	if(m_renderLights.m_bRightFront) {
+	if(m_renderLights.m_bRightFront){
 		CalculateLeanMatrix();
-		DoHeadLightBeam(m_leanMatrix, true);
+		DoHeadLightBeam(VEHICLE_LIGHT_MAIN, m_leanMatrix, true);
 	}
 }
 
diff --git a/src/vehicles/Vehicle.cpp b/src/vehicles/Vehicle.cpp
index d149155..eca9c76 100644
--- a/src/vehicles/Vehicle.cpp
+++ b/src/vehicles/Vehicle.cpp
@@ -33,6 +33,9 @@
 #include "Particle.h"
 #include "WaterLevel.h"
 #include "Timecycle.h"
+#include "Clock.h"
+#include "ZoneCull.h"
+#include "Shadows.h"
 #include "Weather.h"
 #include "Coronas.h"
 #include "SaveBuf.h"
@@ -49,6 +52,9 @@ bool CVehicle::bCheat10;
 bool CVehicle::bHoverCheat;
 bool CVehicle::bAllTaxisHaveNitro;
 bool CVehicle::m_bDisableMouseSteering = true;
+// SA CVehicle::ms_forceVehicleLightsOff: the cutscene flag that suppresses every
+// lamp. Read by DoVehicleLights, which is the only thing that draws them.
+bool CVehicle::bForceVehicleLightsOff = false;
 int32 CVehicle::m_nLastControlInput = KEYBOARD;
 bool CVehicle::bDisableRemoteDetonation;
 bool CVehicle::bDisableRemoteDetonationOnContact;
@@ -132,6 +138,7 @@ CVehicle::CVehicle(uint8 CreatedBy)
 	m_nGunFiringTime = 0;
 	m_nTimeBlocked = 0;
 	bLightsOn = false;
+	m_nOverrideLights = NO_CAR_LIGHT_OVERRIDE;
 	bVehicleColProcessed = false;
 	m_numPedsUseItAsCover = 0;
 	bIsCarParkVehicle = false;
@@ -2353,12 +2360,14 @@ CVehicle::CanBeDriven(void) const
 // it. Alpha fades as the camera moves off the beam axis, so the cone is only
 // visible when you are looking roughly into the light.
 void
-CVehicle::DoHeadLightBeam(CMatrix &vehicleMatrix, bool isRight)
+CVehicle::DoHeadLightBeam(eVehicleLightId lightId, CMatrix &vehicleMatrix, bool isRight)
 {
-	// MAIN lamp pair only; SA looks up DUMMY_LIGHT_FRONT_MAIN here.
-	CVector pointModelSpace = GetDummyPositionObjSpace(DUMMY_LIGHT_FRONT_MAIN);
+	CVector pointModelSpace = GetDummyPositionObjSpace(GetDummyFromLightId(lightId, true));
 
-	if(pointModelSpace.x == 0.0f && pointModelSpace.y == 0.0f && pointModelSpace.z == 0.0f)
+	// A model that has no second lamp quad gets no second beam either.
+	if(lightId == VEHICLE_LIGHT_SECONDARY && pointModelSpace.IsZero())
+		return;
+	if(pointModelSpace.IsZero())
 		return;
 
 	CVector point = vehicleMatrix.GetPosition() + Multiply3x3(pointModelSpace, vehicleMatrix);
@@ -2426,6 +2435,517 @@ CVehicle::DoHeadLightBeam(CMatrix &vehicleMatrix, bool isRight)
 	RwRenderStateSet(rwRENDERSTATECULLMODE, (void*)rwCULLMODECULLBACK);
 }
 
+// ---------------------------------------------------------------------------
+// Optical lights: SA CVehicle::DoVehicleLights and the functions under it
+// ---------------------------------------------------------------------------
+//
+// This is the SA lamp pass, ported whole. The reference draws a vehicle's lamps
+// in one place and keeps the answer in m_renderLights; every consumer - the
+// material pass that swaps the lamp sheets, the beam a bike draws, a second
+// driver reading the state - looks at that answer. The VC code this replaces
+// had two independent blocks inside PreRender, each with its own idea of the
+// colours and the damage rules, and neither of them recorded anything, so:
+//
+//   * the lamp sheet swap could not know which lamps were lit;
+//   * CBike::Render's m_renderLights.m_bRightFront test could never be true;
+//   * the second lamp dummy ("headlights2"/"taillights2") was never used;
+//   * a brake light while the lamps were off and the reversing lights were
+//     drawn as the rear lamp's own red corona rather than as their own effect.
+
+int32
+CVehicle::GetDummyFromLightId(eVehicleLightId lightId, bool isFront)
+{
+	if(lightId == VEHICLE_LIGHT_SECONDARY)
+		return isFront ? DUMMY_LIGHT_FRONT_SECONDARY : DUMMY_LIGHT_REAR_SECONDARY;
+	return isFront ? DUMMY_LIGHT_FRONT_MAIN : DUMMY_LIGHT_REAR_MAIN;
+}
+
+// SA CVehicle::GetVehicleLightsStatus (0x6D55C0).
+//
+// The car's lamps should be on at night, in the hour before and after it, and
+// whenever the weather is bad enough for the random seed of this particular car
+// to be under the foggyness or wetness of the map. A gang member is the one
+// driver who does not bother; SA additionally requires the zone to be gang
+// territory, which VC's population model has no notion of, so the ped type and
+// the seed are the whole test here.
+bool
+CVehicle::GetVehicleLightsStatus(void) const
+{
+	if(pDriver && pDriver->m_nPedType >= PEDTYPE_GANG1 && pDriver->m_nPedType <= PEDTYPE_GANG9 &&
+	   pDriver->m_randomSeed % 2)
+		return false;
+
+	if(CClock::GetIsTimeInRange(21, 6))
+		return true;
+	// SA splits the two boundary hours by the vehicle's own random seed, so
+	// that the cars of a street do not all switch on in the same frame.
+	if(CClock::GetHours() == 20 && CClock::GetMinutes() > (m_randomSeed % 64))
+		return true;
+	if(CClock::GetHours() == 6 && CClock::GetMinutes() < (m_randomSeed % 64))
+		return true;
+
+	const float threshold = (float)m_randomSeed / 50000.0f;
+	if(CWeather::Foggyness > threshold || CWeather::WetRoads > threshold)
+		return true;
+
+	// SA's last clause is "the car stands on a surface darker than five percent
+	// and the cull zone it is in keeps the rain out", i.e. it is in a tunnel or
+	// under a bridge where nothing else would prompt it to switch on. reVC has
+	// no per-vehicle collision lighting sample (SA keeps four, one per corner,
+	// and averages them in CalculateLightingFromCollision), so the brightness
+	// half is approximated from the ambient the time cycle gives the current
+	// hour and weather: that is the light that reaches a surface with nothing
+	// else shining on it, and it falls below the same five percent threshold in
+	// exactly the situations SA's collision sample reports as dark.
+	if(!CCullZones::CamNoRain())
+		return false;
+	const float ambient = (CTimeCycle::GetAmbientRed() + CTimeCycle::GetAmbientGreen() +
+		CTimeCycle::GetAmbientBlue()) * (1.0f / 3.0f);
+	// The time cycle stores 0..255 as a float already scaled to 0..1 in the
+	// current-ambient accessors; five percent of full white is the threshold.
+	return ambient < 0.05f;
+}
+
+// SA CVehicle::DoLightEffectImpl (0x6E1780).
+//
+// One lamp of one pair, on one side. The geometry comes from the dummy the
+// light id names, so a model with one lamp per side lights two quads and a
+// model with two lights four, without any code knowing which it has: the
+// SECONDARY dummy of a model that does not carry one is the zero vector and
+// the lamp returns before it registers anything.
+bool
+CVehicle::DoLightEffect(eVehicleLightId lightId, bool isFront, CMatrix &vehicleMatrix,
+	bool isRight, bool disabledOrAlarm, bool staticEmission)
+{
+	// SA's own constants (0x8D3684 / 0x8D3688): how far the lamp glow spreads
+	// relative to the angle the camera sees it from.
+	const float FRONT_SIZE_ROT_MULT = 0.4f;
+	const float REAR_SIZE_ROT_MULT = 0.2f;
+
+	if(disabledOrAlarm)
+		return false;
+
+	const CDamageManager *damage = GetDamageManager();
+	if(!isFront && damage && (GetModelIndex() == MI_STALLION || GetModelIndex() == MI_SABRE) &&
+	   damage->GetPanelStatus(VEHBUMPER_REAR))
+		// These two models have lamp textures mapped onto their rear bumper;
+		// with the bumper gone there is nothing to light.
+		return false;
+
+	CVector dummyPosObjSpace = GetDummyPositionObjSpace(GetDummyFromLightId(lightId, isFront));
+
+	// The second lamp of a pair exists only on models that model it.
+	if(lightId == VEHICLE_LIGHT_SECONDARY && dummyPosObjSpace.IsZero())
+		return false;
+
+	// Front lamps sit five centimetres ahead of their dummy: a lamp is a
+	// surface, and the coronas must not be swallowed by the glass in front of
+	// it at grazing angles.
+	CVector tweakedDummy = dummyPosObjSpace;
+	if(isFront)
+		tweakedDummy = 0.05f * GetForward() + dummyPosObjSpace;
+
+	// The left lamp of a pair is the right dummy mirrored about the car's
+	// centre line, which is what SA does instead of storing a dummy per lamp.
+	if(!isRight)
+		tweakedDummy.x -= 2.0f * dummyPosObjSpace.x;
+
+	const CVector dummyPosWorldSpace = vehicleMatrix * tweakedDummy;
+
+	CVector dirToCam = TheCamera.GetPosition() - dummyPosWorldSpace;
+	const float distToCam = dirToCam.Magnitude();
+	if(distToCam != 0.0f)
+		dirToCam *= 1.0f / distToCam;
+
+	CVector forward = isFront ? vehicleMatrix.GetForward() : -vehicleMatrix.GetForward();
+	const float angle = DotProduct(dirToCam, forward);
+	const float normAngle = Sqrt(Max(angle, 0.0f));
+
+	const bool isFullsizeTrain = IsSubTrain();
+
+	float intensity, coronaSize;
+	if(isFront){
+		intensity = normAngle * 0.5f + 0.3f;
+		coronaSize = (1.0f - distToCam * (1.0f / 150.0f)) * normAngle * FRONT_SIZE_ROT_MULT;
+	}else{
+		intensity = angle * 0.5f + 0.2f;
+		coronaSize = (1.0f - distToCam * (1.0f / 150.0f)) * angle * REAR_SIZE_ROT_MULT;
+	}
+	if(isFullsizeTrain){
+		const float trainMult = isFront ? 2.0f : 3.0f;
+		intensity = Min(trainMult * intensity, 1.0f);
+		coronaSize = 4.0f * coronaSize;
+	}
+
+	const float lightDistMult = 150.0f * TheCamera.LODDistMultiplier;
+	// The corona id is per lamp, not per car: two cars close together must not
+	// steal each other's coronas, and the four lamps of one car must not steal
+	// each other's either. SA derives it from the vehicle pointer and the lamp
+	// ordinal; the same scheme is used here.
+	const uint32 baseId = (uint32)(uintptr)this;
+	const uint32 lampId = (uint32)lightId * 2 + (isRight ? 1 : 0);
+
+	bool isBraking = false;
+
+	if(angle > 0.0f &&
+	   (TheCamera.Cams[TheCamera.ActiveCam].Mode != CCam::MODE_1STPERSON || this != FindPlayerVehicle())){
+		if(isFront){
+			// A headlight seen almost straight on also draws a thin line of
+			// light, which is what makes it read as a lamp and not a dot.
+			const float fieldAngle = isFullsizeTrain ? 0.85f : 0.9f;
+			if(normAngle > fieldAngle && distToCam < 40.0f){
+				uint8 lineR = 160, lineG = 160, lineB = 140;
+				if(m_pHandlingData->m_nHandlingFlags & VEHICLE_HANDLING_HALOGEN_LIGHTS){
+					lineR = 150;
+					lineG = 150;
+					lineB = 195;
+				}
+				CCoronas::RegisterCorona(baseId + lampId + 4, lineR, lineG, lineB, 255,
+					dummyPosWorldSpace, isFullsizeTrain ? 0.3f : 0.075f, lightDistMult,
+					CCoronas::TYPE_HEADLIGHT, CCoronas::FLARE_NONE, CCoronas::REFLECTION_OFF,
+					CCoronas::LOSCHECK_OFF, CCoronas::STREAK_OFF, normAngle, true, 0.3f);
+			}
+
+			uint8 lightColorR, lightColorG;
+			if(m_pHandlingData->m_nHandlingFlags & VEHICLE_HANDLING_HALOGEN_LIGHTS){
+				lightColorR = (uint8)(190.0f * intensity);
+				lightColorG = (uint8)(intensity * 255.0f);
+			}else{
+				lightColorR = (uint8)(210.0f * intensity);
+				lightColorG = (uint8)(intensity * 195.0f);
+			}
+			// SA passes (r, r, g): the two first channels are the same red
+			// multiplier, the third is the green one, which is what gives the
+			// warm white of a headlight.
+			CCoronas::RegisterCorona(baseId + lampId, lightColorR, lightColorR, lightColorG, 128,
+				dummyPosWorldSpace, coronaSize, lightDistMult,
+				CCoronas::TYPE_HEADLIGHT, CCoronas::FLARE_NONE, CCoronas::REFLECTION_SIMPLE,
+				CCoronas::LOSCHECK_OFF, CCoronas::STREAK_OFF, normAngle, false, 0.5f);
+		}else{
+			uint8 redIntensity = 0;
+			if(pDriver && m_fBrakePedal > 0.0f && !bIsHandbrakeOn){
+				// The driver is on the brakes: the lamp is at its bright
+				// state, and that is what the caller records as "braking".
+				redIntensity = (uint8)(128.0f * intensity);
+				isBraking = true;
+			}else if(staticEmission){
+				// The lamps are on but nobody is braking: the tail lamp still
+				// burns, dimmer. This is the "static part" SA mentions.
+				redIntensity = (uint8)(96.0f * intensity);
+				isBraking = true;
+			}
+			CCoronas::RegisterCorona(baseId + lampId + 8, redIntensity, 0, 0, 128,
+				dummyPosWorldSpace, coronaSize, lightDistMult,
+				CCoronas::TYPE_HEADLIGHT, CCoronas::FLARE_NONE, CCoronas::REFLECTION_SIMPLE,
+				CCoronas::LOSCHECK_OFF, CCoronas::STREAK_OFF, normAngle, false, 0.5f);
+		}
+	}
+
+	// A front lamp is always considered lit (it is the headlight that the
+	// caller is drawing); a rear lamp only when it showed something.
+	return isFront || isBraking;
+}
+
+bool
+CVehicle::DoHeadLightEffect(eVehicleLightId lightId, CMatrix &vehicleMatrix, bool isRight,
+	bool disabledOrAlarm)
+{
+	return DoLightEffect(lightId, true, vehicleMatrix, isRight, disabledOrAlarm, false);
+}
+
+bool
+CVehicle::DoTailLightEffect(eVehicleLightId lightId, CMatrix &vehicleMatrix, bool isRight,
+	bool disabledOrAlarm, bool staticEmission)
+{
+	return DoLightEffect(lightId, false, vehicleMatrix, isRight, disabledOrAlarm, staticEmission);
+}
+
+// SA CVehicle::DoHeadLightReflection (0x6E1720).
+//
+// The pool of light a car throws on the road in front of it. Twin when both
+// sides of the pair are lit, a single one when only one is: the shape is the
+// same road quad, mirrored to the lit side and half as wide.
+void
+CVehicle::DoHeadLightReflection(CMatrix &vehicleMatrix, eVehicleLightsFlags flags,
+	bool includeLeft, bool includeRight)
+{
+	const bool doTwin = (flags & VEHICLE_LIGHTS_TWIN) ? (includeLeft && includeRight) : false;
+	const bool doSingle = !doTwin && (includeLeft || includeRight);
+
+	if(!doTwin && !doSingle)
+		return;
+
+	CVector vehOffset = GetDummyPositionObjSpace(DUMMY_LIGHT_FRONT_MAIN);
+	if(doSingle && includeLeft)
+		vehOffset.x *= -1.0f;
+
+	CVector2D lightFwd2D(vehicleMatrix.GetForward());
+	lightFwd2D.Normalise();
+	// A bike and the quad have their lamp in the middle of the body, so the
+	// pool is one wide quad; a car has two, one per lamp. SA names the quad by
+	// model id (MODEL_QUAD); the SA vehicle set reVC carries has no such model,
+	// so the sub-type - which is what the handling record says - is the test.
+	const bool singleLampBody = IsSubBike() || IsSubQuad() || IsBMX();
+	const float lightSize = (doSingle && singleLampBody)
+		? 1.25f
+		: (doTwin ? vehOffset.x : Abs(vehOffset.x)) * 4.0f;
+	if(lightSize <= 0.0f)
+		return;
+
+	const float offsetDistance = lightSize * 2.0f + 1.0f + vehOffset.y;
+	const CVector2D shdwFront = lightFwd2D * (lightSize * 2.0f);
+	const CVector2D shdwSide = CVector2D(lightFwd2D.y, -lightFwd2D.x) * lightSize;
+	CVector2D lightPos2D = lightFwd2D * offsetDistance;
+	if(doSingle){
+		CVector2D lightRight2D(vehicleMatrix.GetRight());
+		lightRight2D.Normalise();
+		lightPos2D += lightRight2D * vehOffset.x;
+	}
+
+	CVector shadowPos = GetPosition() + CVector(lightPos2D.x, lightPos2D.y, 2.0f);
+	// SA keeps two pool textures: gpShadowHeadLightsTex for the two-lamp pool
+	// of a car and gpShadowHeadLightsTex2 for the single pool of a vehicle with
+	// one lamp, which is why the choice follows doTwin.
+	CShadows::StoreCarLightShadow(this, (int32)((uintptr)this + 2),
+		doTwin ? gpShadowHeadLightsTex : gpShadowHeadLightsTex2,
+		&shadowPos, shdwFront.x, shdwFront.y, shdwSide.x, shdwSide.y,
+		45, 45, 45, 7.0f);
+}
+
+// SA CVehicle::DoReverseLightEffect. The white reversing lamps. They are only
+// drawn with the lamps *off*: with the lamps on the reversing quads are part of
+// the lit sheet, and drawing a corona on top of them would double the light.
+void
+CVehicle::DoReverseLightEffect(CMatrix &vehicleMatrix)
+{
+	if(!(m_fGasPedal < 0.0f || m_nCurrentGear == 0))
+		return;
+
+	CVector lookVector = GetPosition() - TheCamera.GetPosition();
+	lookVector.Normalise();
+	if(DotProduct(lookVector, GetForward()) <= 0.0f)
+		return;	// the camera is in front of the car, nothing to see
+
+	const CVector dummyPos = GetDummyPositionObjSpace(DUMMY_LIGHT_REAR_MAIN);
+	CVector lightR = vehicleMatrix * dummyPos;
+	CVector lightL = lightR;
+	lightL -= GetRight() * 2.0f * dummyPos.x;
+
+	const CDamageManager *damage = GetDamageManager();
+	const float distMult = 50.0f * TheCamera.LODDistMultiplier;
+	if(damage == nil || damage->GetLightStatus(VEHLIGHT_REAR_LEFT) == LIGHT_STATUS_OK)
+		CCoronas::RegisterCorona((uint32)((uintptr)this + 16), 120, 120, 120, 255,
+			lightL, 1.2f, distMult,
+			CCoronas::TYPE_STAR, CCoronas::FLARE_NONE, CCoronas::REFLECTION_ON,
+			CCoronas::LOSCHECK_OFF, CCoronas::STREAK_ON, 0.0f);
+	if(damage == nil || damage->GetLightStatus(VEHLIGHT_REAR_RIGHT) == LIGHT_STATUS_OK)
+		CCoronas::RegisterCorona((uint32)((uintptr)this + 17), 120, 120, 120, 255,
+			lightR, 1.2f, distMult,
+			CCoronas::TYPE_STAR, CCoronas::FLARE_NONE, CCoronas::REFLECTION_ON,
+			CCoronas::LOSCHECK_OFF, CCoronas::STREAK_ON, 0.0f);
+}
+
+// Which lamps this vehicle has at all, and whether the two sides of a lamp pair
+// are separate geometry. SA passes the twin flag from the handling record; reVC
+// asks the model instead, because a converted model can carry a second lamp
+// quad that the handling line knows nothing about, and because a model with a
+// single quad per pair must not have its left lamp sheet lit for a lamp that
+// has no geometry.
+CVehicle::eVehicleLightsFlags
+CVehicle::GetVehicleLightsFlags(void) const
+{
+	eVehicleLightsFlags flags = VEHICLE_LIGHTS_NONE;
+	const CVehicleModelInfo *mi = (const CVehicleModelInfo*)CModelInfo::GetModelInfo(GetModelIndex());
+	if(mi == nil)
+		return flags;
+
+	// A bike has one headlight in the middle of the fork, so there is no left
+	// half to light separately; SA records the lamp as the right-hand one and
+	// never sets the twin flag for a two-wheeler.
+	if(!IsBike() && !IsBMX() && mi->m_vehicleStruct.IsDummyActive(DUMMY_LIGHT_FRONT_MAIN))
+		flags = (eVehicleLightsFlags)(flags | VEHICLE_LIGHTS_TWIN);
+
+	return flags;
+}
+
+// SA CVehicle::DoVehicleLights (0x6E1A60).
+void
+CVehicle::DoVehicleLights(CMatrix &vehicleMatrix, eVehicleLightsFlags flags)
+{
+	if(bForceVehicleLightsOff)
+		return;
+
+	// 1. Should the lamps be on?
+	const bool lightsStatus = GetVehicleLightsStatus();
+	if(lightsStatus != bLightsOn && GetStatus() != STATUS_WRECKED){
+		if(GetStatus() != STATUS_ABANDONED || IsSubTrain()){
+			bLightsOn = lightsStatus;
+		}else if(bLightsOn){
+			// An abandoned car keeps its lamps on only while the player can
+			// see them: once it is far away there is nothing to light.
+			const CVector &camPos = TheCamera.GetPosition();
+			if(Abs(camPos.x - GetPosition().x) + Abs(camPos.y - GetPosition().y) > 100.0f)
+				bLightsOn = false;
+		}
+	}
+
+	bool forceOn = false;
+	bool forceOff = false;
+
+	// 2. A script's standing order wins over the time of day.
+	if(m_nOverrideLights == FORCE_CAR_LIGHTS_OFF)
+		bLightsOn = false;
+	else if(m_nOverrideLights == FORCE_CAR_LIGHTS_ON)
+		forceOn = true;
+
+	// 3. An alarm blinks the lamps, whatever the time of day.
+	if(IsAlarmOn()){
+		if(CTimer::GetTimeInMilliseconds() & 0x100)
+			forceOn = true;
+		else
+			forceOff = true;
+	}
+
+	// A car with its engine off has no lamps: the battery is the engine.
+	if(!bEngineOn){
+		m_nRenderLightsFlags = 0;
+		return;
+	}
+
+	// 4. The one model whose lamps have to physically come up first.
+	if(GetModelIndex() == MI_INFERNUS && GetStatus() != STATUS_WRECKED){
+		// SA's ZR350 pop-up headlights (0.69813174f = 40 degrees). The model
+		// reVC maps to that slot is the same shape; while the pods are still
+		// coming up the lamps stay dark.
+		const float popUpTarget = 0.69813174f;
+		if(bLightsOn || forceOn || IsAlarmOn()){
+			m_fPropRotate = Min(m_fPropRotate + CTimer::GetTimeStep() * 0.01f, popUpTarget);
+			if(m_fPropRotate < popUpTarget){
+				m_nRenderLightsFlags = 0;
+				return;
+			}
+		}else{
+			m_fPropRotate = Max(m_fPropRotate - CTimer::GetTimeStep() * 0.01f, 0.0f);
+		}
+	}
+
+	// 5. A smashed lamp draws nothing. VEHICLE_LIGHTS_IGNORE_DAMAGE is for the
+	// passes that must show the vehicle as it is (a mirror, a shadow map).
+	struct LightOkFn {
+		CVehicle *self;
+		eVehicleLightsFlags flags;
+		bool operator()(eVehicleLightsFlags disabledFlag, eLights light) const
+		{
+			if(flags & disabledFlag)
+				return false;
+			if(flags & VEHICLE_LIGHTS_IGNORE_DAMAGE)
+				return true;
+			// A lamp can only be smashed on a vehicle that carries a damage
+			// manager; a bike's lamp is part of its bodywork.
+			const CDamageManager *damage = self->GetDamageManager();
+			if(damage == nil)
+				return true;
+			return damage->GetLightStatus(light) == LIGHT_STATUS_OK;
+		}
+	} isLightOk = { this, flags };
+
+	const bool lightOkFR = isLightOk(VEHICLE_LIGHTS_DISABLE_FRONT, VEHLIGHT_FRONT_RIGHT);
+	const bool lightOkFL = isLightOk(VEHICLE_LIGHTS_DISABLE_FRONT, VEHLIGHT_FRONT_LEFT);
+	const bool lightOkRR = isLightOk(VEHICLE_LIGHTS_DISABLE_REAR, VEHLIGHT_REAR_RIGHT);
+	const bool lightOkRL = isLightOk(VEHICLE_LIGHTS_DISABLE_REAR, VEHLIGHT_REAR_LEFT);
+
+	// 6. Draw one pair on both sides, then the second dummy of the pair.
+	// This is the whole fault of the VC code: only the right-hand lamp was
+	// ever drawn, the left was a mirror of its position, and the second dummy
+	// was never consulted, so m_renderLights could not describe a car.
+	struct RenderLightsFn {
+		CVehicle *self;
+		CMatrix &vehicleMatrix;
+		eVehicleLightsFlags flags;
+		void operator()(bool isFrontLight, bool disabledOrAlarmR, bool disabledOrAlarmL,
+			bool staticEmission)
+		{
+			bool active = isFrontLight
+				? self->DoHeadLightEffect(VEHICLE_LIGHT_MAIN, vehicleMatrix, true, disabledOrAlarmR)
+				: self->DoTailLightEffect(VEHICLE_LIGHT_MAIN, vehicleMatrix, true, disabledOrAlarmR, staticEmission);
+			if(isFrontLight)
+				self->m_renderLights.m_bRightFront = active;
+			else
+				self->m_renderLights.m_bRightRear = active;
+			// The second lamp of the pair is drawn whenever the first one
+			// drew something; DoLightEffect itself returns early for a model
+			// that has no such dummy.
+			if(active){
+				if(isFrontLight)
+					self->DoHeadLightEffect(VEHICLE_LIGHT_SECONDARY, vehicleMatrix, true, disabledOrAlarmR);
+				else
+					self->DoTailLightEffect(VEHICLE_LIGHT_SECONDARY, vehicleMatrix, true, disabledOrAlarmR, staticEmission);
+			}
+
+			if(flags & VEHICLE_LIGHTS_TWIN){
+				active = isFrontLight
+					? self->DoHeadLightEffect(VEHICLE_LIGHT_MAIN, vehicleMatrix, false, disabledOrAlarmL)
+					: self->DoTailLightEffect(VEHICLE_LIGHT_MAIN, vehicleMatrix, false, disabledOrAlarmL, staticEmission);
+				if(isFrontLight)
+					self->m_renderLights.m_bLeftFront = active;
+				else
+					self->m_renderLights.m_bLeftRear = active;
+				if(active){
+					if(isFrontLight)
+						self->DoHeadLightEffect(VEHICLE_LIGHT_SECONDARY, vehicleMatrix, false, disabledOrAlarmL);
+					else
+						self->DoTailLightEffect(VEHICLE_LIGHT_SECONDARY, vehicleMatrix, false, disabledOrAlarmL, staticEmission);
+				}
+			}else{
+				// A single-sided model has no left lamp of its own: the right
+				// one is the whole lamp, so it is recorded for the left too.
+				// Without this a model with one quad per pair would leave the
+				// left lamp sheet dark while its corona burns.
+				if(isFrontLight)
+					self->m_renderLights.m_bLeftFront = self->m_renderLights.m_bRightFront;
+				else
+					self->m_renderLights.m_bLeftRear = self->m_renderLights.m_bRightRear;
+			}
+		}
+	} RenderLights = { this, vehicleMatrix, flags };
+
+	if(forceOff){
+		m_nRenderLightsFlags = 0;
+		return;
+	}
+
+	if(!bLightsOn && !forceOn){
+		// Lamps off. Only the dynamic half of the tail lamp is processed: a
+		// brake light and the reversing lights work without the lamps, which
+		// is the whole point of them.
+		RenderLights(false, !lightOkRR, !lightOkRL, false);
+		DoReverseLightEffect(vehicleMatrix);
+		return;
+	}
+
+	// Lamps on.
+	RenderLights(true, !lightOkFR, !lightOkFL, false);
+	RenderLights(false, !lightOkRR, !lightOkRL, true);
+
+	const bool isSubTrain = IsSubTrain();
+	if(!isSubTrain)
+		DoHeadLightReflection(vehicleMatrix, flags, lightOkFL, lightOkFR);
+
+	// 7. The light a lamp throws on the road. SA adds one directional light
+	// for the headlights of every lit car, and one at the back only while the
+	// driver is actually braking with the handbrake off.
+	if(lightOkFR || lightOkFL)
+		CPointLights::AddLight(CPointLights::LIGHT_DIRECTIONAL, vehicleMatrix.GetPosition(),
+			vehicleMatrix.GetForward(), 20.0f, 1.0f, 1.0f, 1.0f,
+			m_vecMoveSpeed.MagnitudeSqr2D() < SQR(0.45f) ? CPointLights::FOG_NORMAL : CPointLights::FOG_NONE,
+			false);
+
+	if((lightOkRR || lightOkRL) && m_fBrakePedal > 0.0f && !bIsHandbrakeOn && pDriver)
+		CPointLights::AddLight(CPointLights::LIGHT_DIRECTIONAL,
+			GetPosition() + -4.0f * GetForward(), -vehicleMatrix.GetForward(),
+			10.0f, 0.1f, 0.02f, 0.02f, CPointLights::FOG_NONE, false);
+}
+
 eVehicleAppearance
 CVehicle::GetVehicleAppearance(void)
 {
@@ -3377,6 +3897,55 @@ CVehicle::ApplyRemapTexture(void)
 	CVehicleModelInfo::ms_pRemapTexture = m_pRemapTexture;
 }
 
+// SA CVehicle::SetupRender (0x6D64F0).
+//
+// Everything a vehicle puts onto its shared model for the duration of one draw,
+// in SA's order:
+//
+//  1. the four paint channels of this car, so the body markers resolve to its
+//     colours and not to the last car of the same model;
+//  2. the lamp flags of this car, which the material pass reads to decide which
+//     lamp sheets are lit;
+//  3. the paintjob texture of this car;
+//  4. the walk itself, which does the swaps.
+//
+// reVC used to do 1, 3 and 4 inside each vehicle's own Render and never did 2 at
+// all, which is why the lamps could not light. The split also matters for the
+// alpha pass: CRenderer draws the sorted alpha atomics of a vehicle after its
+// Render has returned, so the swaps must be made here, outside Render, and only
+// be undone by ResetAfterRender.
+void
+CVehicle::SetupRender(void)
+{
+	CVehicleModelInfo *mi = (CVehicleModelInfo*)GetModelInfo();
+	if(mi == nil)
+		return;
+
+	// The lamps first: the material callback consults ms_lightsOn for every
+	// material it walks, and a stale set would light the wrong lamps.
+	CVehicleModelInfo::SetupLightFlags(this);
+
+	// Then the colours and the paintjob the body markers are resolved against.
+	mi->SetVehicleColour(m_currentColour1, m_currentColour2, m_currentColour3, m_currentColour4);
+	ApplyRemapTexture();
+
+	// And the walk. It is unconditional, unlike the old paintjob-only call: a
+	// car with no paintjob can still have lamps to swap.
+	mi->SetEditableMaterials();
+}
+
+// SA CVehicle::ResetAfterRender (0x6D0E20). Undo everything SetupRender did to
+// the shared model. Called once the entity and its alpha atomics have been
+// drawn; without it the next car of the same model inherits this one's paintjob
+// and lit lamps.
+void
+CVehicle::ResetAfterRender(void)
+{
+	CVehicleModelInfo *mi = (CVehicleModelInfo*)GetModelInfo();
+	if(mi)
+		mi->RestoreEditableMaterials();
+}
+
 // ---- State API for an external controller -------------------------------
 //
 // See the block in Vehicle.h for the contract. These are the parts that cannot
diff --git a/src/vehicles/Vehicle.h b/src/vehicles/Vehicle.h
index 61c274e..430a64e 100644
--- a/src/vehicles/Vehicle.h
+++ b/src/vehicles/Vehicle.h
@@ -137,6 +137,16 @@ enum eLights
 	VEHLIGHT_REAR_RIGHT,
 };
 
+// SA eVehicleOverrideLightsState (Vehicle.h). A script's standing order for a
+// vehicle's lamps: it wins over the time of day, and it is also what makes a
+// car show its lamps at noon or keep them off at night.
+enum eVehicleOverrideLightsState : uint8
+{
+	NO_CAR_LIGHT_OVERRIDE = 0,
+	FORCE_CAR_LIGHTS_OFF  = 1,
+	FORCE_CAR_LIGHTS_ON   = 2,
+};
+
 enum
 {
 	CAR_PIECE_BONNET = 1,
@@ -248,7 +258,11 @@ public:
 	uint8 bIsLocked: 1; // Is this guy locked by the script (cannot be removed)
 	uint8 bEngineOn: 1; // For sound purposes. Parked cars have their engines switched off (so do destroyed cars)
 	uint8 bIsHandbrakeOn: 1; // How's the handbrake doing ?
-	uint8 bLightsOn: 1; // Are the lights switched on ?
+	// SA CVehicle::vehicleFlags.bLightsOn. What the lamp pass last decided:
+	// the lamps are on, so the lamp sheets swap to the lit one. Written by
+	// CVehicle::DoVehicleLights from GetVehicleLightsStatus, overridden by a
+	// script forcing them on or off.
+	uint8 bLightsOn: 1;
 	uint8 bFreebies: 1; // Any freebies left in this vehicle ?
 
 	uint8 bIsVan: 1; // Is this vehicle a van (doors at back of vehicle)
@@ -318,6 +332,10 @@ public:
 			uint8 m_bLeftRear   : 1;
 		} m_renderLights;
 	};
+	uint8 m_nOverrideLights;   // eVehicleOverrideLightsState
+	// SA CVehicle::ms_forceVehicleLightsOff. With it set no vehicle draws a
+	// lamp at all, whatever its own state says - the cutscene code uses it.
+	static bool bForceVehicleLightsOff;
 	uint32 m_nGunFiringTime;    // last time when gun on vehicle was fired (used on boats)
 	uint32 m_nTimeOfDeath;
 	uint16 m_nTimeBlocked;
@@ -489,10 +507,23 @@ public:
 	// -1 removes the paintjob.
 	void SetRemap(int32 remapIndex);
 	// Applies the pending remap texture to the model's editable materials and
-	// hands it to the model for the duration of one Render. Called by the
-	// automobile render path, and again when the texture was swapped.
+	// hands it to the model for the duration of one Render. Called by
+	// SetupRender, and again when the texture was swapped.
 	void ApplyRemapTexture(void);
 
+	// SA CVehicle::SetupRender (0x6D64F0) and CVehicle::ResetAfterRender
+	// (0x6D0E20). The pair brackets the drawing of one vehicle: SetupRender
+	// paints the model with this car's four colours, publishes its paintjob,
+	// copies its lamp flags out and swaps the materials that carry state;
+	// ResetAfterRender puts every one of them back. SA calls them from
+	// CRenderer::RenderOneNonRoad, around the entity's own Render and the
+	// alpha pass, which is why they live here and not inside a vehicle's
+	// Render: the sorted alpha list draws atomics of the same vehicle after
+	// Render has returned, and those atomics must still wear the swapped
+	// materials.
+	void SetupRender(void);
+	void ResetAfterRender(void);
+
 	// ---- State API for an external controller ---------------------------
 	//
 	// Everything above is what the game itself calls. The accessors below are
@@ -576,6 +607,18 @@ public:
 	void SetEngineOn(bool on) { bEngineOn = on; }
 	bool AreLightsOn(void) const { return bLightsOn; }
 	void SetLightsOn(bool on) { bLightsOn = on; }
+	// A script's standing order on the lamps, in SA's three states. Note that
+	// AreLightsOn is the *result* of the last lamp pass, not the order: with
+	// FORCE_CAR_LIGHTS_OFF the pass turns the lamps off on the next frame,
+	// while the order itself stays until it is cleared.
+	eVehicleOverrideLightsState GetOverrideLights(void) const
+	{
+		return (eVehicleOverrideLightsState)m_nOverrideLights;
+	}
+	void SetOverrideLights(eVehicleOverrideLightsState state)
+	{
+		m_nOverrideLights = (uint8)state;
+	}
 	bool IsSirenOrAlarmOn(void) const { return m_bSirenOrAlarm; }
 	void SetSirenOrAlarm(bool on) { m_bSirenOrAlarm = on; }
 	// How long the horn has been sounding, in milliseconds; 0 means silent.
@@ -699,11 +742,85 @@ public:
 
 	eVehicleAppearance GetVehicleAppearance(void);
 
+	// ---- Optical lights (SA CVehicle::DoVehicleLights family) -----------
+	//
+	// SA draws a car's lamps in one place, CVehicle::DoVehicleLights
+	// (0x6E1A60), and keeps the answer - which of the four lamps actually lit
+	// - in m_renderLights. That answer is read twice: by the render setup to
+	// swap the lamp sheets (CVehicleModelInfo::SetupLightFlags) and by
+	// CBike::Render to decide on the headlight beam. The functions below are
+	// that one place, split the way SA splits it, so what gets drawn and what
+	// the material pass does cannot disagree.
+	//
+	// SA eVehicleLightsFlags (Vehicle.h). The flags say how the lamp pass is
+	// to be run for the vehicle, not what state the vehicle is in.
+	enum eVehicleLightsFlags
+	{
+		VEHICLE_LIGHTS_NONE          = 0,
+		VEHICLE_LIGHTS_TWIN          = 1,  // both sides of a lamp pair lit
+		VEHICLE_LIGHTS_IGNORE_DAMAGE = 4,  // draw even a smashed lamp
+		VEHICLE_LIGHTS_DISABLE_FRONT = 16, // no front lamps this pass
+		VEHICLE_LIGHTS_DISABLE_REAR  = 32, // no rear lamps this pass
+	};
+
+	// SA eVehicleLightId (Vehicle.h): the first and the second lamp dummy of
+	// a pair, i.e. "headlights"/"headlights2" and "taillights"/"taillights2".
+	enum eVehicleLightId
+	{
+		VEHICLE_LIGHT_MAIN      = 0,
+		VEHICLE_LIGHT_SECONDARY = 1,
+	};
+
+	// SA CVehicle::DoVehicleLights (0x6E1A60). Decides whether the lamps are
+	// on, draws the coronas, the light shadows and the point lights, and fills
+	// m_renderLights with what was actually lit. This replaces the VC family
+	// (separate headlight and taillight blocks inside CAutomobile::PreRender),
+	// which differed in colour, size, damage handling, the second lamp dummy
+	// and the reversing lights.
+	void DoVehicleLights(CMatrix &vehicleMatrix, eVehicleLightsFlags flags);
+	// The flags DoVehicleLights is called with for this vehicle on an ordinary
+	// frame: twin lamps for a car whose two sides have separate geometry, plus
+	// the two disables for a vehicle that has no lamps at its front or rear.
+	eVehicleLightsFlags GetVehicleLightsFlags(void) const;
+	// SA CVehicle::GetVehicleLightsStatus (0x6D55C0). Whether the car's lamps
+	// *should* be on: night, poor weather, or the shadows the cull zone puts on
+	// the road. The result is cached in bLightsOn by DoVehicleLights.
+	bool GetVehicleLightsStatus(void) const;
+
+	// SA CVehicle::DoLightEffectImpl (0x6E1780): one lamp of one pair. Returns true when the lamp registered a corona, which
+	// is what the caller stores in m_renderLights.
+	bool DoLightEffect(eVehicleLightId lightId, bool isFront, CMatrix &vehicleMatrix,
+		bool isRight, bool disabledOrAlarm, bool staticEmission);
+	// SA CVehicle::DoHeadLightEffect (0x6E0A50) / DoTailLightEffect (0x6E1780).
+	bool DoHeadLightEffect(eVehicleLightId lightId, CMatrix &vehicleMatrix, bool isRight,
+		bool disabledOrAlarm);
+	bool DoTailLightEffect(eVehicleLightId lightId, CMatrix &vehicleMatrix, bool isRight,
+		bool disabledOrAlarm, bool staticEmission);
+	// SA CVehicle::DoReverseLightEffect. The white reversing lamps, drawn only
+	// when the car is in reverse and its lamps are off: with the lamps on the
+	// reversing quad is already in the lit sheet.
+	void DoReverseLightEffect(CMatrix &vehicleMatrix);
+	// SA CVehicle::DoHeadLightReflection (0x6E1720) and its two entry points.
+	void DoHeadLightReflection(CMatrix &vehicleMatrix, eVehicleLightsFlags flags,
+		bool includeLeft, bool includeRight);
+
+	// The lamp dummy of a light id in model space: DUMMY_LIGHT_FRONT_MAIN /
+	// DUMMY_LIGHT_FRONT_SECONDARY / DUMMY_LIGHT_REAR_MAIN /
+	// DUMMY_LIGHT_REAR_SECONDARY.
+	static int32 GetDummyFromLightId(eVehicleLightId lightId, bool isFront);
+
+	// SA CDamageManager::GetLightStatus needs a damage manager, and only an
+	// automobile has one. The lamp pass asks through here so a bike's lamp -
+	// which is part of its bodywork and cannot be smashed separately - is
+	// treated as intact instead of dereferencing nothing.
+	virtual class CDamageManager *GetDamageManager(void) { return nil; }
+	virtual const class CDamageManager *GetDamageManager(void) const { return nil; }
+
 	// SA CVehicle::DoHeadLightBeam (0x6E0E20). A five-vertex additive cone
-	// anchored at the headlight dummy and aimed along the bonnet. In SA only
-	// CBike::Render calls it; cars get the corona and the point light and
-	// nothing else, so this is not wired into CAutomobile.
-	void DoHeadLightBeam(CMatrix &vehicleMatrix, bool isRight);
+	// anchored at one of the two headlight dummies and aimed along the bonnet.
+	// In SA only CBike::Render calls it; cars get the corona and the point
+	// light and nothing else, so this is not wired into CAutomobile.
+	void DoHeadLightBeam(eVehicleLightId lightId, CMatrix &vehicleMatrix, bool isRight);
 
 	// SA helper: the light dummy in model space. DUMMY_LIGHT_FRONT_MAIN for the
 	// primary lamp pair, DUMMY_LIGHT_FRONT_SECONDARY for the second one.
```
