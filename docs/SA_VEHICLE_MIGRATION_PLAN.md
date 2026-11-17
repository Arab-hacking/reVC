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

## Stage 2 — vehicle identity and class hierarchy

`eVehicleType` (`VEHICLE_TYPE_IGNORE` −1 … `VEHICLE_TYPE_TRAILER` 11) replaces
the model-info `VEHICLE_TYPE_CAR/BOAT/TRAIN/HELI/PLANE/BIKE` enumeration, which
has a different order and no monster-truck, quad, BMX, trailer, fake-heli or
fake-plane class.

- `CVehicle::m_nVehicleType` / `m_nVehicleSubType`, both derived from the
  handling record and the model info, replacing the single `m_vehType`.
- `CVehiclePool` becomes `CPool<CVehicle, CHeli>`; the largest allocation unit
  must cover `CHeli` before `CVehiclePool::New` hands out smaller objects.
- New classes with canonical constructors: `CMonsterTruck`, `CQuadBike`,
  `CBmx`, `CTrailer`. `CVehicle::IsCar/IsBoat/IsTrain/IsHeli/IsPlane/IsBike`
  are re-expressed in terms of the new type word.
- Model-info admission (`CVehicleModelInfo::SetModelInfo`) maps the handling
  record plus the model to a `eVehicleType` instead of the VC table.

## Stage 3 — automobile physics

- Suspension: per-wheel `CAutomobile::ProcessSuspension` over
  `CPhysical::ApplySpringCollision`/`ApplySpringDamp`, driven by the SA
  `m_fSuspensionForceLevel`, `m_fSuspensionDampingLevel`,
  `m_fSuspensionHighSpdComDamp`, `m_fSuspensionBiasBetweenFrontAndRear` and the
  `VEHICLE_HANDLING_WHEEL_*` flags instead of the VC bounding-box heuristics.
- Wheels: SA wheel radius and `m_fMassRecip` based traction, ABS and
  handbrake handling, per-wheel collision flags.
- Braking/traction split by `m_fBrakeBias`, `m_fTractionBias`,
  `m_fTractionLoss` and `m_fBrakeDeceleration` as the reference does.
- Air resistance and the stabiliser torque use the collision model's bounding
  box, not `m_vecCentreOfMass`; the centre of mass only feeds the inertia
  offset.

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
