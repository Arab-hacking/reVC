# SA-only car model port — stage 1, not a completed SA renderer

## Scope and reference

User-selected scope: loading/displaying car models, not a port of vehicle
physics, handling, traffic AI, or scripts.

Reference: https://github.com/gta-reversed/gta-reversed
at `943c2a37cf964afccaa5ff1e5db4d75c17043bcb`.
Read `source/game_sa/Models/VehicleModelInfo.cpp` (`SetClump`,
`PreprocessHierarchy`, `SetAtomicRendererCB`, `SetEditableMaterialsCB`),
`source/game_sa/VisibilityPlugins.cpp` (`RenderWheelAtomicCB`), and the
vehicle pipeline file layout. Algorithms adapted to reVC/librw interfaces;
no SA executable addresses/hooks are used. Credit to gta-reversed and its
Plugin-SDK/GTA Community contributors.

## Implemented

* `VEHICLE_TYPE_CAR` enters a separate `PreprocessSACarHierarchy`. It is not
  detected by absence of `_hi`. Non-car categories retain their old path.
* Car atomics follow SA's classifier: `_vlo` is low detail; other names get
  opaque/alpha high-detail callbacks, including the windscreen special case.
  Damage variants are initially hidden. `_lo` does **not** mean deletion in
  SA's car classifier.
* `wheel_rf_dummy` supplies the main wheel. Its children collapse onto the
  main frame and the atomic is cloned onto the other wheel dummies, as in
  the reference. No separate VC wheel model is substituted in this path.
* SA wheel callback renders the embedded atomic directly. It never asks a
  VC simple-model wheel for alternate LOD geometry. Car wheel render scale
  is 1; mesh radius remains a bridge to the intentionally retained physics.
* Both loose-file and two-stage car loads require valid embedded COL2/COL3.
  Missing/broken collision refuses the car rather than silently using the
  stock replacement model's VC collision. External legacy COL loading skips
  car slots. This intentionally breaks compatibility with original VC cars
  which do not carry SA embedded collision.
* Fixed the proven use-after-free: parsed SA collision arrays are views into
  a temporary buffer, now copied before freeing it. `numVertices` is set.
  The parser checks lengths/offset overflow/count representability, finite
  bounds/volumes and vertex extents; face indices determine vertex count,
  not padding between arrays. Zero unused offsets are valid for sphere-only
  files. COL3/2 chunk payloads are capped at 16 MiB.

## Still required before claiming full SA model support

* SA four independent colour slots and per-instance material save/restore.
  Existing code still maps tertiary/quaternary paint into two VC slots.
* SA remap textures, light texture/state handling, dirt and custom plates.
* Native SA car env/spec material plugins and their complete GL3 pipeline,
  without the current heuristic material classification/fallback env maps.
* SA vehicle metadata, hierarchy flags/dummies and specialized car variants,
  upgrade attachments, double rear wheels. Current hierarchy uses reVC IDs
  and component flags as the engine interface; this is not a wholesale port
  of SA vehicle structures.
* Remove the remaining engine-side signed-int16 collision count limits if
  larger models are required; currently out-of-range data is rejected safely.
* Real-asset tests and Windows/GL runtime validation of wheels, damage parts,
  alpha sorting, material isolation across instances, streaming and reloads.

SA also has six extras in its vehicle structure: the number six is not itself
proof of a VC-specific workaround. Full rendering parity cannot be inferred
from the presence of an SA-only geometry-loading branch.

## Tests

* `sa_vehicle_collision_load_tests`: real `LoadSAVehicleColModel`, RenderWare
  stream/model registry stubs and a real collision pool/arrays. Checks buffer
  lifetime, reloads, vertex counts, padding, malformed offsets/indices,
  COL2 and sphere-only files. ASan/UBSan (vptr disabled for headless stubs)
  passes. Restoring the old free-before-conversion order reproduces ASan
  `heap-use-after-free`.
* `sa_car_render_policy_tests`: calls the real car atomic classifier with RW
  stubs; checks plain names, `_hi`, `_lo`, `_vlo`, alpha, windscreen and damage.
* Existing native COLL loader test still passes. Changed sources pass GCC
  syntax checks for desktop GL3 and Android. These are not GPU/gameplay tests.

## BR analysis: material channels (follow-up)

Reference: user-supplied vehicles.txt ZIP, `vehicles/Анализ/02-data-formats.md`
(section 4) and `04-textures.md` (11.2–11.5). The reference contains provisional
interpretations; do not treat its native addresses/layouts as portable C++ ABI.

- Preserve four distinct SA RGB paint-marker channels during material discovery.
  Channels 3/4 have their own bounded, terminated lists and reflection registration.
- Add an explicit four-colour model-material API, preserving alpha. Existing
  two-colour gameplay calls touch only channels 1/2; no physics/handling/entity
  state changes are made. Until an explicit four-colour caller supplies them,
  channels 3/4 remain neutral white, not aliases of 1/2.
- BR body remap recognition uses a case-sensitive `remap` prefix, excluding
  `remaptoner` first. `plate_remap`/`platem_remap` and toner overlays must not
  acquire body-paint reflection heuristics just because they contain `remap`.

Validation: standalone VehicleMaterialChannelsTests and desktop/Android syntax
checks for VehicleModelInfo.cpp. No GPU or real BR asset run performed.

This is not a complete BR port. Palette pixel replacement, per-instance four-colour
persistence/selection, overlay composition, light atlas switching, plate generation,
and carmods loading/attachment remain unimplemented. No render callbacks, shaders,
physics or handling code changed in this follow-up. Implementing BR shader-specific
palette/toner composition requires separately scoped rendering work.

## BR shared resources and upgrade streaming

Reference: supplied `vehicles.txt`, analysis 04 §§11.2–11.5, analysis 02 §3,
and the actual `_data/data_carmods.dat` (not merely its six-column summary).

Implemented:
- Explicitly resolve BR shared BTX resources into the vehicle dictionary through
  the existing TexDB loader: remapvehicles[/snow], remaptoner[/snow], lights off/on,
  left/right indicators, plate_remap and platem_remap. TexDB and dictionary refs
  are independent; unavailable BTX resources are skipped without synthetic plates.
- Create private 16x16 RGBA palette textures from an explicit 256-entry table and
  private solid-colour toner textures. Callers own the returned ref. No cached
  BTX texture or raster is edited. The UV-to-colour layout is NOT guessed.
- Index bounded `data/carmods.dat` in common.zip/common.bpc and expose validated
  metadata by vehicle. Parse UTF-8 BOM, commas, optional/multiple stock names,
  sparse IDs, exact duplicate records; reject conflicting duplicates and malformed
  numbers/names. The supplied file has 2711 rows, 2704 distinct definitions (seven
  identical duplicates), including model IDs larger than the engine's model table.
- Expose nonblocking request/create/release APIs for selected component models.
  Validate vehicle/type/model-table bounds before streamer access; no truncation
  of BR external IDs. Instances pin model/TXD refs and teardown removes frame/clump
  links. Global cleanup runs before world/model shutdown. Owners must release an
  attached component BEFORE destroying its parent clump.

Not implemented / not claimed:
- These APIs do not select upgrades automatically, register missing component IDEs,
  remap external model IDs, attach to dummies or hide stock geometry automatically.
- Private palette/toner factories do not by themselves implement BR multitexture
  palette/overlay shaders. There is no per-vehicle palette binding/persistence yet.
- Loading the light/plate resources is not automatic light-state switching, plate
  text generation or a BR material shader. Existing rendering remains unchanged.
- No physics, handling, shader or render-callback changes in this stage.

Validation:
- BRVehicleResourcesTests including the actual supplied carmods table, ASan/UBSan.
- DesktopBpcTexturesTests, direct ASTC and CPU fallback, with real common.bpc
  carmods indexing/reading and vehicle-specific lookup fixtures.
- Desktop and Android syntax checks of CustomModels.cpp and Game.cpp.
- No real-asset GPU/Windows playtest; resource factories, instance lifetime and
  shared vehicle dictionary population have syntax coverage, not runtime coverage.

## debug-4.log: stock vehicle collision regression

The supplied Windows log reports revision 677b39f, repeated missing-embedded-COL
failures for stock IMG vehicle names (including police and packer), then an
access violation reading 0xC at executable RVA 0x2E8A1A. There is no stack trace
or matching PDB in the attachment; the precise faulting function is NOT established.
The last audio log lines alone do not implicate the audio system.

Correct the demonstrated loader regression rather than requiring all mounted
resources to be SA DFFs:
- External COLL readers accept vehicle collisions unless validated model-owned
  collision already exists. Previously they skipped every CAR even when it was
  a stock IMG model with no embedded collision.
- Vehicle preparation tries embedded COL first. A custom car can instead use
  validated archive COL/CLS looked up by its own model name, never an unrelated
  stock collision by replacement ID. That collision is protected from stock reloads.
- Stock streamed cars can use their already-owned external collision and no longer
  fail simply because their DFF has no SA extension. Custom cars without either
  valid source still fail rather than silently inheriting stock collision.
- No physics, handling, rendering or audio changes.

Regression coverage invokes the real external COLL loader for a CAR, verifies its
collision is installed, then verifies an external reload cannot replace embedded
SA geometry. Existing malformed/unaligned/lifetime checks remain. Native COLL and
SA vehicle collision tests pass, including SA adapter ASan/UBSan; Android syntax
check passes. Windows crash resolution still requires a repeat run; use the matching
EXE/PDB and crash.dmp if RVA 0x2E8A1A recurs.

## debug-5.log / crash (2).txt: optional wheel atomic dereference

The attachment named .txt is an MDMP, not a text report. Log revision: 8b404e9.
Fault: write to 0x2 at RVA 0x1E1E20, SP 0xAF4F97F558. The original AMD64
CONTEXT preserved on the crash-handler stack (flags 0x10005F) has RCX=0,
RDX=0, RIP=0x7FF770971E20. Return-address candidate at original SP is exe RVA
0x2BAB98. The saved context also has R14=199, consistent with the just-streamed
sparrow model, but this register alone is not a symbolized model identification.

A concrete matching unsafe path exists in CAutomobile's helicopter setup:
`RpAtomicSetFlags((RpAtomic*)GetFirstObject(wheelDummy), 0)`. RW atomic flags
are at byte offset 2. Empty wheel dummies are legal with embedded geometry; a
null first object therefore produces exactly this kind of write. Do not synthesize
VC wheel meshes merely to satisfy this assumption.

Use a checked optional-node operation in vehicle construction and Fix(): skip a
missing frame, empty dummy or non-atomic object; preserve the existing flags=0
operation for valid atomics. No physics/handling algorithm, shader, render callback
or normal rendering path is changed. (Fix is the vehicle repair path, not save
restoration.)

Tests exercise the exact helper with missing/empty/non-atomic/valid nodes and
repeat calls; ASan/UBSan pass. Automobile.cpp desktop and Android syntax checks
pass (existing pointer-size warning remains). Matching Windows CI EXE/PDB download
was unavailable from the sandbox artifact host, so this is a verified null-path
fix consistent with the dump, not a fully symbolized Windows reproduction. The
next Windows run must confirm the observed crash no longer occurs.
