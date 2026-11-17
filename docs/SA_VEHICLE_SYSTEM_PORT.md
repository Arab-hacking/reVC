# SA vehicle-system migration

## Scope change

The user explicitly selected **the whole SA vehicle system**, superseding the
previous models-only scope and restrictions on physics, handling and rendering.
Acceptance is not satisfied by hiding VC branches behind a macro or by changing
only model names. Stock VC vehicle assets and VC algorithms must eventually leave
the active vehicle path. This document does not claim that this has happened.

## Pinned behavioral reference

Repository: https://github.com/gta-reversed/gta-reversed
Revision: `56a67498c822c7bf6fe07f663ddef7a6be3504f1`

Consulted in this stage:
- `source/game_sa/cTransmission.cpp`, `.h`
- `source/game_sa/cHandlingDataMgr.cpp`
- `source/game_sa/tHandlingData.h`
- `source/game_sa/Entity/Vehicle/Automobile.cpp`, `Bike.cpp`

Native addresses are provenance, not functions to call in reVC. Retain GTA
Community/gta-reversed/Plugin-SDK attribution when adapting their work.

## Completed: SA transmission calculations

`src/vehicles/Transmission.cpp` now implements SA behavior for:
- Gear ratios (0x6D0460): half-gear velocity offset, SA up/down thresholds.
- Simple traffic shifting (0x6D0530): no shift beyond top gear or below reverse.
- Drive acceleration (0x6D05E0): floating-point gear curve, independent handling
  boost flags, reverse torque, perfect-handling and nitros multipliers, engine
  inertia, free-rev state, 0.85 smoothing and 0.05 overspeed torque falloff.
- Shifts discard inertia and cheat overrides in the same iteration as SA.
- No recursive VC acceleration formula, integer gear-division bug, or shared static
  force/ratio scratch state remains in this implementation.

The transmission flags field is now 32-bit and engine inertia is represented.
Cars and bikes pass per-instance inertia state plus drive-wheel contact to the
SA calculation. Automobile engine-off/aircraft drive gating follows the reference.
Vehicle inertia starts at (0,0), as in the reference automobile constructor.
Compatible-save loading resets this transient state without changing save bytes.
The old five-argument VC acceleration API has been removed, not retained as a
fallback. Nitros force is supported by the transmission API but no nitros-control
subsystem has been ported yet.

Defensive differences outside valid SA input: reject nonfinite input/invalid gear
counts, bound invalid restored gears and shift iteration, handle zero-inertia
free-rev without NaN. Supported forward gears are 1..5 (six array slots including
reverse); do not trust the contradictory 1..6 header comment in the reference.

## Completed: SA handling data

`src/vehicles/cHandlingDataMgr.cpp` replaces the VC handling manager and parser:

- The 210-name table in `tVehicleType.h` is the handling id space, exposed as
  `enum eHandlingId`; the VC `HANDLING_*` macros and the old name table are gone.
- Vehicle (36 columns), bike (15, `!`), flying (21, `$`) and boat (14, `%`)
  records are parsed in the reference column order and validated before they are
  stored; a rejected line leaves the record byte-identical.
- `ConvertDataToGameUnits`/`ConvertBikeDataToGameUnits` apply the reference unit
  conversions, derive `m_fMassRecip`, `m_fBuoyancyConstant`, the flat, cruise and
  reverse velocities, and initialise the gear ratios.
- Records are reached through `m_pHandlingData`, `m_pFlyingHandlingData`,
  `m_pBikeHandlingData` and `m_pBoatHandlingData` with the canonical SA member
  names. `HandlingMgr.{h,cpp}` and `Transmission.{h,cpp}` are deleted;
  `cTransmission.{h,cpp}` and `cHandlingDataMgr.{h,cpp}` replace them.
- `tests/HandlingDataMgrTests.cpp` covers parsing, unit conversion, flag
  reachability and lookup bounds; it and `tests/TransmissionTests.cpp` pass.

## Tests performed

- `SATransmissionTests` compiles the real Transmission.cpp with a timer stub.
  Covers ratios, shift bounds, torque, cheats, reverse, inertia, separate instance
  states, timestep scaling, invalid counts, zero inertia and nonfinite input.
- ASan/UBSan run of that target passes.
- A temporary differential harness compiled the pinned gta-reversed algorithms
  alongside this implementation. Only field/class names, static game-address
  bindings and includes were adapted for the harness. **172800 cases** matched
  acceleration, selected gear and both inertia values (tolerance 1e-6): five gear
  counts, four boost combinations, all valid starting gears, 120 speeds, three
  throttle values, three cheat modes and two contact states. Defensive invalid
  inputs were excluded from reference comparisons.
- Desktop/Android syntax checks of Transmission, HandlingMgr, Automobile, Bike and
  Vehicle pass; existing integer/pointer-size warnings remain.
- No Windows runtime or vehicle-dynamics playtest performed for this stage.

## Superseded by the staged plan

The handling registry/parser is no longer VC: `cHandlingDataMgr` now carries the
SA tables and parser (see *Completed: SA handling data* above). The remaining
work is tracked stage by stage in `docs/SA_VEHICLE_MIGRATION_PLAN.md`:

1. **Vehicle identity and classes:** the model-info `eVehicleType` enumeration,
   `CVehicle::m_vehType`, the pool allocation unit and the missing
   `CMonsterTruck`/`CQuadBike`/`CBmx`/`CTrailer` classes.
2. **Contact/suspension/traction/braking:** Automobile/Bike contact loops and the
   remaining bounding-box heuristics are not SA ports yet.
3. **Model admission/spawn:** native VC IMG vehicle definitions and streaming
   compatibility remain.
4. **Rendering/material application:** the SA CarFX/env/spec/light/plate renderer
   and per-instance BR palette composition are not complete.
5. **Gameplay integration:** door tasks, occupants, damage, AI/autopilot, audio,
   animations, weapons, trailers, hydraulics/upgrades, scripts, save/load and
   streaming dependencies.

Local vehicle implementation is roughly 15600 lines across Vehicle, Automobile,
Bike, Boat, Heli, Plane, HandlingMgr and Transmission alone, excluding dependencies.
Replacing files wholesale would import unresolved SA engine globals and ABI
assumptions rather than a working port. Each migrated subsystem must have numeric
or runtime regression tests and remove, rather than silently retain, its old path.
