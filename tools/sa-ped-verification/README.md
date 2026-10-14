# SA pedestrian / IFP regression tests

Run from the checkout (Linux, C++11 compiler and CMake):

```sh
tools/sa-ped-verification/run.sh
# Or CMAKE=/path/to/cmake CXX=clang++ tools/sa-ped-verification/run.sh
```

All binaries and fixtures stay in `build/sa-tests`. No proprietary game assets,
OpenGL context or audio SDK is required. The NULL librw backend is built locally.
The fake RenderWare wrapper uses PS2 vertex *declarations* only; no PS2 renderer
is started. `null_device.h` supplies its otherwise missing window-parameter type.

Coverage:

* `test_ifp.cpp`: allocation-free ANP2, ANP3, mobile ANP3 and ANPK validation;
  every truncated prefix, trailing garbage, excessive counts, invalid types,
  NaN/backwards timestamps, VC/SA tag classification and 10,000 deterministic
  mutations. Runs with AddressSanitizer and UndefinedBehaviorSanitizer.
* `test_runtime.cpp`: links the real `CAnimManager`, animation sequences,
  associations, `CustomModels`, `CustomZip`, RwHelper and librw. Checks actual
  decoded quaternion/time/translation values, mixed float/packed sequences,
  ANPK bank overrides, hierarchy on the clump root, native VC vs extended SA
  skeleton detection and rebinding a differently ordered/larger skeleton.
* Actual loose-file discovery from `Custom/PED.IFP`, mobile header normalization,
  explicit `Custom/sa_ped.ifp` (ANPK) and authoritative `REVC_CUSTOM_DIR`.
* Runs the runtime suite both with and without `ANIM_COMPRESSION`; syntax-checks
  the pedestrian model integration unit as well.

The only game-service stubs are logging and unused streaming/file-system fallback
branches. Animation parsing, skeleton lookup and sequence binding are not mocked.
These are synthetic regression fixtures, not a full gameplay/visual test.

## Format references

The compact reader follows the runtime layouts in
[gta-reversed's LoadAnimFile_ANP23](https://github.com/gta-reversed/gta-reversed/blob/master/source/game_sa/Animation/AnimManager.cpp)
and the extension tags in
[eBoneTag.h](https://github.com/gta-reversed/gta-reversed/blob/master/source/game_sa/Enums/eBoneTag.h).
Unlike ANPK, float compact translation frames store `quaternion, time, xyz`, and
the quaternion is already in runtime orientation. Packed frames use scales
4096, 60 and 1024. Animation storage is normalized before optional VC cache
compression, so one hierarchy never mixes incompatible sequence representations.
