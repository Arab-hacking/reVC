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

## Still NOT migrated

This is an intentionally explicit checklist, not completed functionality:

1. **Handling registry/parser and units:** SA/BR parser, stable full-width registry,
   archive loading and canonical SA conversion now implemented (see below).
   Legacy boot data remains for stock definitions; removing stock admission and
   every legacy consumer is NOT complete. Raw SA model and handling words are
   separate; a named temporary compatibility view remains for unported consumers.
2. **Contact/suspension/traction/braking:** Automobile/Bike contact loops are not
   SA ports. SA entries now use supplied turn mass and SA drag-based air resistance
   (including Boat); old Dimension-based stabilization still needs migration.
3. **Vehicle identity and classes:** reVC model constants, aircraft-as-automobile
   cases, boats, trains and special classes are still present. Their constructors,
   model-node assumptions and subtype dispatch must move together.
4. **Model admission/spawn:** native VC IMG vehicle definitions and streaming
   compatibility remain. Simply rejecting these DFFs reintroduces unloaded-model
   assumptions in scripts, car generators and constructors. A source-authoritative
   SA/BR registry and failure-aware spawn sites must precede removing that bridge.
5. **Rendering/material application:** previous material/resource stages are not
   a complete SA CarFX/env/spec/light/plate renderer or per-instance BR palette
   composition. Model-only changes do not satisfy this item.
6. **Gameplay integration:** door tasks, occupants, damage, AI/autopilot, audio,
   animations, weapons, trailers, hydraulics/upgrades, scripts, save/load and
   streaming dependencies still require reference-by-reference migration.

Local vehicle implementation is roughly 15600 lines across Vehicle, Automobile,
Bike, Boat, Heli, Plane, HandlingMgr and Transmission alone, excluding dependencies.
Replacing files wholesale would import unresolved SA engine globals and ABI
assumptions rather than a working port. Each migrated subsystem must have numeric
or runtime regression tests and remove, rather than silently retain, its old path.

## SA handling registry implementation

- `SAHandlingData.h`: bounded transactional 36-column parser with independent
  32-bit flag words, 15/14/21-value auxiliary schemas and retained BR extensions.
  Accepts the supplied double BOM, comments, final lines without newline and
  canonical RCRAIDER wind typo. Rejects truncation, duplicate names, nonfinite
  values, embedded NUL, invalid gear counts and oversized input. Crane and animation
  rows are explicitly counted/skipped; their semantics are NOT ported.
- Real supplied BR table: **565 base records, 23 bikes, 16 boats, 24 flying**.
  Boat suspension columns intentionally are not constrained like road-car columns:
  supplied valid boats include negative traction, bias >1, reversed limits.
- `LoadSAHandlingData` installs stable maps atomically, preserves matching native
  handling IDs and allocates new IDs beyond `NUMHANDLINGS`. No uint8 truncation
  in auxiliary lookups; no reload that could invalidate live handling pointers.
  Unknown names return -1 instead of aliasing the first dynamic entry.
- Canonical mass, turn mass, drag, engine inertia, high-speed damping, flags,
  animation group and three extra flying fields stored. SA conversion from
  pinned `cHandlingDataMgr::ConvertDataToGameUnits` (0x6F5080), including drive
  split, terminal velocity iteration, max-speed flag, RC reverse and bike reverse.
- Common ZIP/BPC and vehicles ZIP data lookup admits `data/handling.cfg` with
  a 1 MiB ceiling. CustomWorld resolves named SA records, requires matching
  auxiliary type data and refuses generic VC handling substitution once SA
  handling is installed. Native same-name definitions receive their SA handling.
- `SAHandlingLoadTests` exercises real manager/parser/transmission, numeric
  conversions, immutable pointer stability, malformed input and >255 auxiliary
  IDs. Optional fixture argument verifies every supplied base record resolves.
  Standalone and ASan/UBSan runs pass with the actual BR table. Full desktop
  syntax checks pass; full Windows asset runtime remains unverified.

This is not a claim that all vehicle logic has been migrated. In particular,
stock models, legacy flag consumers, class dispatch, contact dynamics, resource
rendering and gameplay integrations in the remaining checklist still exist.
