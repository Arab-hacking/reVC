#!/bin/sh
# Builds the test rig for the custom folder feature.
#
#   ./build.sh && ./test_custom archive.zip [reference.dff]
#
# The rig reads an archive exactly like the game does (CustomZip.cpp) and runs
# the conversion chain of the game on what is inside (br/*), so a broken
# archive reader or a broken container shows up here, without the game.
set -e
cd "$(dirname "$0")"
g++ -O2 -g -w -std=c++11 -DCUSTOM_MODELS=1 \
    -I shim -I ../../src/core -I ../../src/extras/custom -I ../../src/extras/custom/br \
    -I ../../src/extras/custom/br/third_party \
    test_custom.cpp \
    ../../src/extras/custom/CustomZip.cpp ../../src/extras/custom/CustomCol.cpp \
    ../../src/extras/custom/br/third_party/astc_decomp.cpp \
    -o test_custom
echo "built ./test_custom"
g++ -O2 -g -w -std=c++11 -DCUSTOM_MODELS=1 \
    -I shim -I ../../src/core -I ../../src/renderer -I ../../src/extras/custom -I ../../src/extras/custom/br \
    -DNUMHOURS=24 -DNUMWEATHERS=7 \
    test_custom2.cpp shim/tc_shim.cpp \
    ../../src/extras/custom/CustomTimecycle.cpp \
    -o test_custom2
echo "built ./test_custom2"
