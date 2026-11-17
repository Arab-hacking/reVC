# SA vehicle-system migration plan

Reference: <https://github.com/gta-reversed/gta-reversed> at
`56a67498c822c7bf6fe07f663ddef7a6be3504f1`.

The migration replaces the Vice City vehicle implementation with the San
Andreas one. Nothing is kept behind a version switch: a stage is finished only
when the VC code path it replaces is deleted, not disabled.

## Ground rules

- Canonical gta-reversed names only. `CVehicle`, `CAutomobile`, `CBike`,
  `CBoat`, `CHeli`, `CPlane`, `CTrain`, `cHandlingDataMgr`, `cTransmission`,
  `tHandlingData`, and the matching file names.
- No game-tagged identifiers (`SA*`/`VC*` prefixes, `*_SA.cpp`, `ProcessSA`).
- No dead code, no commented-out VC blocks, no `#ifdef` version selection.
- Every stage leaves `src/` compiling and the tests passing.

## Stage 1 — handling data and transmission (done)

`cHandlingDataMgr` owns the handling tables and parses `DATA\HANDLING.CFG`:

| table | entries | record |
| --- | --- | --- |
| `HandlingData` | 210 | `tHandlingData`, 36 columns |
| `BikeHandlingData` | 13 | `tBikeHandlingData`, `!` lines, 15 columns |
| `FlyingHandlingData` | 24 | `tFlyingHandlingData`, `$` lines, 21 columns |
| `BoatHandlingData` | 12 | `tBoatHandlingData`, `%` lines, 14 columns |

- Order of `VehicleNames[]` is the handling id, exposed as `enum eHandlingId`
  (`HANDLING_LANDSTAL` … `HANDLING_RCRAIDER`). VC's `HANDLING_*` macros are gone.
- Records are validated before they are written: a rejected line leaves the
  stored record byte-identical.
- Units: file km/h and seconds are converted once on load
  (`ConvertDataToGameUnits`, `ConvertBikeDataToGameUnits`).
- `cTransmission` is the SA gearbox: gear ratio table, gear selection for
  traffic and the drive-acceleration curve with the reference constants
  (`TRANSMISSION_*`).
- Consumers reach the records through `m_pHandlingData`,
  `m_pFlyingHandlingData`, `m_pBikeHandlingData`, `m_pBoatHandlingData` and the
  canonical member names (`m_fMass`, `m_fMassRecip`, `m_vecCentreOfMass`,
  `m_Transmission`, `m_nModelFlags`, `m_nHandlingFlags`, …).

Tests: `tests/TransmissionTests.cpp`, `tests/HandlingDataMgrTests.cpp`.

## Stage 2 — vehicle identity and class hierarchy (done)

`eVehicleType` (`VEHICLE_TYPE_IGNORE` −1 … `VEHICLE_TYPE_TRAILER` 11) lives in
`eVehicleType.h` and replaces the model-info
`VEHICLE_TYPE_CAR/BOAT/TRAIN/HELI/PLANE/BIKE` enumeration, which had a different
order and no monster-truck, quad, BMX, trailer, fake-heli or fake-plane class.
`CVehicleModelInfo::ms_vehicleDescs` is indexed by it, one node table per class. The header is added together with its first user so no
unreferenced file is left behind.

- `CVehicle::m_nVehicleType` / `m_nVehicleSubType` replace the single
  `m_vehType`; the `IsAutomobile`/`IsBike`/`IsBoat`/… and `IsSub*` predicates
  come straight from the reference, and the VC `IsCar` accessor is gone.
- `CVehiclePool` keeps `CAutomobile` as its entry type: it is the largest class
  the engine constructs, so every derived instance fits a slot.
- `CFileLoader::LoadVehicleObject` accepts the whole SA type column and
  `CCarCtrl::GetNewVehicleDependingOnCarModel` instantiates the class, so the
  VC "boat / bike / else car" ladders are gone from every spawn site.
- `CVehicle::GetVehicleAppearance` follows the reference model-flag mask.
- Save/load maps the legacy on-disk type words (`0..5`) in both directions.

Still missing from this stage: the classes themselves (`CMonsterTruck`,
`CQuadBike`, `CBmx`, `CTrailer`) are declared in the type word and built as
their nearest implemented parent until stage 4 adds them.

## Stage 3 — automobile physics (in progress)

Done so far:

- Springs and dampers: `CPhysical::ApplySpringCollision`,
  `ApplySpringCollisionAlt` and `ApplySpringDampening` carry the SA formulas and
  the 0.25 / 0.998 limits the executable keeps at `0x8CD7A0` and `0x8CD7A4`.
  A spring reports the damping force it can carry, the damper is clamped by it.
- `CAutomobile::DoBurstAndSoftGroundRatios` owns the per-wheel compression
  (missing wheel, burst tyre, sand, offroad flags); `CAutomobile::ProcessSuspension`
  owns the spring/damper stage, the hydraulic and Vortex tweaks, and the
  roll-back-onto-wheels step. `CBike::ProcessControl` uses the same order.
- The Rhino carries the eight extra track suspension lines the reference builds
  in `SetupSuspensionLines`, with their lengths stored in the rear door angles.
- `CPhysical::ApplyAirResistance` is the reference version: the speed lost per
  frame is `1 - |speed| * m_fAirResistance` to the power of the time step,
  multiplied by 2.5 inside the cull zones flagged `ExtraAirResistance` for
  automobiles and bikes; the turn speed is damped by 0.99.
- The flight arcade model is gone with `CVehicle::FlyingControl` and
  `eFlightModel`: the aeroplane and helicopter handling belong to `CPlane` and
  `CHeli` (stage 4), so the automobile keeps only the rotor spin-up, the blade
  collision and the helicopter weapons. The dodo and flying-car cheats went with
  it.

Still open in this stage:

- `SURFACE_RAILTRACK` only exists in the SA surface table. The repository still
  loads the 35-entry VC `surface.dat`, so the rail-bed wheel drop cannot be
  ported until `CSurfaceTable` moves to the SA 179-entry table; the rest of
  `DoBurstAndSoftGroundRatios` is done.

Remaining work:

- The brake and traction bias in `ProcessControl` still carries the VC
  `2 * m_fBrakeBias` / `2 - m_fBrakeBias` split with the `// looks like a bug`
  note; the reference uses `2 * bias` on both axles. Changing it is a handling
  change, so it is left for a stage that can be driven and compared.
- The stabiliser torque uses the collision model's bounding box, not
  `m_vecCentreOfMass`; the centre of mass only feeds the inertia offset.

## Stage 4 — other classes

In order, each deleting its VC counterpart:

1. `CBike` — bike lean, wheelie/stoppie, `tBikeHandlingData` limits.
2. `CBoat` — `tBoatHandlingData` thrust, anchors, wake audio hookup.
3. `CHeli` and `CPlane`, then `CFlyingHandlingData` aerodynamics for both.
4. `CTrain` — track, bogies, `VEHICLE_TYPE_TRAIN` only; the `GTA_TRAIN` and
   `EXPLODING_AIRTRAIN` switches go away with it.
5. `CMonsterTruck`, `CQuadBike`, `CBmx`, `CTrailer`.
6. `CBuoyancy` for the generic floating path.

## Stage 5 — models and rendering

- `CVehicleModelInfo` on the SA layout: four colour slots, `DamageAtomicModelInfo`,
  `VisibilityPlugins` panel/door/light visibility.
- Embedded COL2/COL3 collision, `GetModelInfo` size checks.
- Wheel, dummy and upgrade hierarchy tables per `eVehicleType`.

## Stage 6 — save, pools and gameplay

- `CVehicleSaveStructure` replaces the VC per-class Save/Load bodies; `Pools.cpp`
  and `save/GenericGameStorage.cpp` follow the new pool type.
- Doors, occupants and `CPed` entry/exit tasks; `CWorld`/`CStreaming`
  registration; `CModelInfo` queries.
- AI (`CCarAI`, autopilot), audio (`cAudioManager` vehicle params), weapons,
  scripts, car generators and replay.

## Verification per stage

`src/` has no CMake build in this environment, so each stage is gated by:

```
g++ -std=c++11 -fsyntax-only -DLIBRW -DRW_GL3 -DLIBRW_SDL2 -DCMAKE_BUILD \
    -DUSE_OUR_VERSIONING -DAUDIO_OAL <include flags> <file>
```

over every `src/*/*.cpp`, plus a manual `g++` link of the pure-logic tests
(`tests/TransmissionTests.cpp`, `tests/HandlingDataMgrTests.cpp`).
