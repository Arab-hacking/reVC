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
