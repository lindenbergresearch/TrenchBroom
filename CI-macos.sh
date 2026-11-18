#!/bin/bash
set -euo pipefail

# Qt remains on 5.15 for migration milestone A. Native libraries are built
# by CPM with the same compiler, architecture and deployment target as the app.
TB_SOURCE_DIR=$(cd "$(dirname "$0")" && pwd)
TB_BUILD_DIR=${TB_BUILD_DIR:-"$TB_SOURCE_DIR/cmake-build-cpm-qt5"}
TB_QT_PREFIX=${TB_QT_PREFIX:-"$(brew --prefix qt@5)"}
TB_BUILD_TYPE=Release
TB_ASAN=OFF
if [[ ${TB_DEBUG_BUILD:-false} == true ]]; then
    TB_BUILD_TYPE=Debug
    TB_ASAN=ON
fi

"$TB_QT_PREFIX/bin/qmake" -v
cmake --version
pandoc --version
cmake -S "$TB_SOURCE_DIR" -B "$TB_BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE="$TB_BUILD_TYPE" \
    -DCMAKE_PREFIX_PATH="$TB_QT_PREFIX" \
    -DTB_ENABLE_ASAN="$TB_ASAN" \
    -DTB_RUN_MACDEPLOYQT=ON \
    -DTB_SUPPRESS_PCH=ON \
    -DTB_ENABLE_CCACHE=ON \
    -DTB_BUILD_REGRESSION_TESTS=ON
cmake --build "$TB_BUILD_DIR" --target TrenchBroom tb-regression-tests --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-8}"
ctest --test-dir "$TB_BUILD_DIR" -R '^tb-regression-tests$' --output-on-failure

if [[ ${TB_PACKAGE:-true} == true && $TB_ASAN == OFF ]]; then
    (cd "$TB_BUILD_DIR" && cpack && ./app/generate_checksum.sh)
fi

echo "Shared libraries used:"
otool -L "$TB_BUILD_DIR/app/TrenchBroom.app/Contents/MacOS/TrenchBroom"
echo "Binary type:"
file "$TB_BUILD_DIR/app/TrenchBroom.app/Contents/MacOS/TrenchBroom"
