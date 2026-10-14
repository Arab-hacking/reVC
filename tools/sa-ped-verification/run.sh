#!/usr/bin/env bash
# No game assets or graphics/audio SDK needed. All outputs stay in build/.
set -euo pipefail
cd "$(dirname "$0")/../.."
CXX=${CXX:-g++}
CMAKE=${CMAKE:-cmake}
OUT="$PWD/build/sa-tests"
mkdir -p "$OUT"
"$CXX" -std=c++11 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  tools/sa-ped-verification/test_ifp.cpp -o "$OUT/ifp"
"$OUT/ifp"
"$CMAKE" -S vendor/librw -B "$OUT/librw" -DLIBRW_PLATFORM=NULL -DLIBRW_TOOLS=OFF -DCMAKE_BUILD_TYPE=Debug
"$CMAKE" --build "$OUT/librw" -j4
includes=(-Ivendor/librw)
while IFS= read -r d; do includes+=("-I$d"); done < <(find src -type d)
flags=(-std=c++11 -g -w -DLIBRW -DRWDEVICE=ps2 -DCMAKE_NO_AUTOLINK
       -include tools/sa-ped-verification/null_device.h -ffunction-sections -fdata-sections)
sources=(src/animation/{AnimManager,AnimBlendHierarchy,AnimBlendSequence,AnimBlendAssociation,AnimBlendAssocGroup,AnimBlendNode,AnimBlendClumpData,RpAnimBlend,Bones}.cpp
         src/extras/custom/{CustomModels,CustomZip}.cpp src/math/Quaternion.cpp src/rw/{RwHelper,MemoryMgr,NodeName}.cpp src/fakerw/fake.cpp
         tools/sa-ped-verification/test_runtime.cpp)
for mode in plain compressed; do
  mkdir -p "$OUT/$mode"
  extra=(); if [[ $mode == compressed ]]; then extra=(-DANIM_COMPRESSION); fi
  for file in "${sources[@]}"; do
    "$CXX" "${flags[@]}" "${extra[@]}" "${includes[@]}" -c "$file" -o "$OUT/$mode/$(basename "$file").o"
  done
  "$CXX" -Wl,--gc-sections "$OUT/$mode/"*.o "$OUT/librw/src/librw.a" -o "$OUT/$mode/runtime"
  "$OUT/$mode/runtime"
done
# Compile touched integration units not linked into the headless harness.
for file in src/extras/custom/CustomModels.cpp src/modelinfo/PedModelInfo.cpp; do
  "$CXX" "${flags[@]}" "${includes[@]}" -fsyntax-only "$file"
done
