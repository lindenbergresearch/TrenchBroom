#!/bin/bash
set -euo pipefail

TB_SOURCE_DIR=$(cd "$(dirname "$0")" && pwd)
TB_BUILD_DIR=${TB_BUILD_DIR:-"$TB_SOURCE_DIR/build"}
cmake --version
pandoc --version
cmake -S "$TB_SOURCE_DIR" -B "$TB_BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DTB_SUPPRESS_PCH=ON -DTB_ENABLE_CCACHE=ON -DTB_BUILD_REGRESSION_TESTS=ON
cmake --build "$TB_BUILD_DIR" --target TrenchBroom tb-regression-tests --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-4}"
ctest --test-dir "$TB_BUILD_DIR" -R '^tb-regression-tests$' --output-on-failure
(cd "$TB_BUILD_DIR" && cpack && ./app/generate_checksum_deb.sh)
ldd "$TB_BUILD_DIR/app/trenchbroom"
