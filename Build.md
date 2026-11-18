# Building this TrenchBroom fork

The editor now uses **Qt 6.10+ and CPM.cmake**. The macOS Apple Silicon build,
focused regression suite and native UI/render/audio checks are validated.
Migration results and estimates are in [Qt6-CPM-Roadmap.md](doc/Qt6-CPM-Roadmap.md).

## Prerequisites

- A C++20 compiler and its platform SDK.
- CMake **3.28 or newer**, Ninja, Git and Pandoc.
- Qt **6.10 or newer**, including Core, Widgets, SVG, Multimedia, Network, Test,
  OpenGL and OpenGLWidgets. The validated version is **6.10.1**.
- OpenGL development libraries and zlib (provided by the macOS SDK).
- Optional: `ccache` for reusing compiler results.

Qt and system OpenGL are installed separately. CPM downloads and builds Assimp,
Catch2, fmt, FreeImage, FreeType, GLEW, miniz and tinyxml2 from verified source
archives. Exact versions, source URLs and SHA-256 hashes are in
[`cmake/dependencies/`](cmake/dependencies). Catch2 remains on version 2.
FreeImage uses the CMake-enabled fork used by original TrenchBroom.
Boost is not required by this fork's source code.

Use a **fresh Qt6 build directory**. Neither a Qt5 build cache nor the former
vcpkg toolchain can be reused. CMake rejects those configurations early.
The active build requires no vcpkg checkout or installed vcpkg packages.
The obsolete submodule link, manifest and overlays have been removed. Existing
local dependency directories and their changes are preserved and ignored by Git.

## macOS (validated on Apple Silicon)

Install Xcode / Command Line Tools and the build tools, for example:

```sh
brew install cmake ninja pandoc qt@6 ccache
```

Configure, build and run the migration regression suite from the repository root:

```sh
cmake --preset qt6-cpm \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6)" \
  -DTB_RUN_MACDEPLOYQT=ON
cmake --build --preset qt6-cpm --parallel 8
ctest --preset qt6-cpm
open cmake-build-cpm-qt6/app/TrenchBroom.app
```

`TB_RUN_MACDEPLOYQT=ON` copies Qt frameworks and plugins into the app bundle,
resolves transitive plugin dependencies, then signs and verifies the generated
bundle. The default `TB_MAC_CODESIGN_IDENTITY=-` creates a local ad-hoc signature
without a certificate. Deployment errors fail the build, even when macdeployqt
returns zero. Each deployment replaces its generated framework/plugin output;
source resources and the installed Qt are never modified.

For faster development builds, omit deployment; the app then uses the installed
Qt. The presets do not hardcode a Homebrew prefix or architecture. Qt5 and Qt6
builds must have different directories. `qt6-cpm-debug` is the corresponding
Debug configure/build/test preset.

The editor defaults to `CMAKE_OSX_DEPLOYMENT_TARGET=14.0`. **A lower compiler
target does not lower the minimum OS of prebuilt dependencies.** The locally
installed Homebrew QtCore requires macOS 14, while QtSvg and QtMultimedia require
macOS 26. This local bundle therefore cannot be advertised as macOS-14-compatible.
To distribute to older systems, use a complete Qt SDK built for the intended OS,
then configure a fresh build with the matching deployment target. Qt 6.10 itself
supports macOS 13+; that does not describe every Homebrew binary. Set
`CMAKE_OSX_ARCHITECTURES` explicitly when needed. Intel/universal builds and
notarization are separate validation work.

The equivalent scripted build is:

```sh
TB_PACKAGE=false ./CI-macos.sh
```

`CI-macos.sh` also accepts `TB_BUILD_DIR`, `TB_QT_PREFIX`, `TB_DEBUG_BUILD=true`
and `CMAKE_BUILD_PARALLEL_LEVEL`. Packaging is enabled by default for release
builds. The script uses the editor and focused regression suite.

## CLion

Use the `qt6-cpm` / `qt6-cpm-debug` presets, or create corresponding profiles:

| Setting | Release example |
| --- | --- |
| Generator | Ninja |
| Build type | Release |
| Build directory | `cmake-build-cpm-qt6` (fresh; Debug uses a separate directory) |
| CMake options | `-DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt` |
| CMake toolchain file | None; remove the former vcpkg option |

The prefix must point to **Qt6**, replacing any previous `qt@5` prefix. A separate
Qt6 directory has been configured successfully with CLion's bundled CMake 4.3.1
and Ninja. Existing Qt5 build directories can remain as references; do not
select those profiles when building this revision.

## Caches and repeat builds

Two independent caches are used by default:

| Cache | Default directory | Purpose |
| --- | --- | --- |
| CPM | `.cache/cpm` | Downloaded/extracted, pinned dependency sources |
| ccache | `.cache/ccache` | Compiler results, when `ccache` is installed |

Both are ignored by Git and survive removal of a build directory. A new build
still configures and links its own targets. `ccache` validates compiler options,
headers and SDK inputs before returning a cached result. Precompiled-header
builds may be uncacheable; `-DTB_SUPPRESS_PCH=ON` trades PCH compilation speed for
broader compiler-cache reuse.

Override the source cache with `-DCPM_SOURCE_CACHE=/path/to/cache` (or the
`CPM_SOURCE_CACHE` environment variable on the initial configure). Override the
compiler cache with `-DTB_CCACHE_DIR=/path/to/cache`. Disable the compiler launcher
with `-DTB_ENABLE_CCACHE=OFF`. No cache is needed for correctness.

```sh
CCACHE_DIR="$PWD/.cache/ccache" ccache --show-stats
```

An initial configure needs network access. Once the pinned sources are present,
a fresh build directory can be configured offline:

```sh
cmake -S . -B cmake-build-cpm-offline -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6)" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
```

Do not edit files inside the shared source cache. Dependency changes should be
explicit patches or new version/hash pins so subsequent builds are repeatable.

## Regression targets

`tb-regression-tests` includes existing regressions for themes, fonts/icons,
actions/toolbars, Inspector docking, Layer selection, Prefabs, WAD properties,
History/Undo, view state and archive caches. Qt6-specific checks cover map text
encodings (including incomplete characters), legacy QSS font weights, shortcut serialization and fractional input
coordinates. Dependency checks exercise the real image/model/font loaders.

CTest runs **223 test cases / 75,793 assertions** with isolated temporary settings
and the offscreen Qt platform. It does not change personal editor settings.
The font/vector-icon/Qt6 subset also passes with `QT_SCALE_FACTOR=1.5` and `2`
(29 cases / 3,240 assertions at each scale).
To run a subset:

```sh
QT_QPA_PLATFORM=offscreen \
  cmake-build-cpm-qt6/regression/tb-regression-tests '[qt6]'
```

Three additional native tests need a logged-in desktop, OpenGL and an audio
backend. They are hidden from the offscreen suite and have also passed locally:

```sh
QT_QPA_PLATFORM=cocoa \
  cmake-build-cpm-qt6/regression/tb-regression-tests '[.native-gl],[.native-audio]'
```

These exercise context lifetime, floating OpenGL widgets, and real audio decoding,
once/loop playback and stop/temporary-file cleanup. The audio test is muted.
Additional native smoke checks use the actual editor, property editing, Inspector
docking/floating, theme changes and SVG selection. The historical `common/test`
and benchmark targets remain disabled because several files use older project
APIs; this is not a claim that the entire historical suite has been ported.

## Windows and Linux

The dependency definitions and static library targets are shared across platforms.
Supply a separately installed Qt6 prefix with `CMAKE_PREFIX_PATH`. Ninja builds
need the appropriate compiler environment. Windows now uses MSVC 2022/x64 and
`windeployqt` from the selected Qt installation instead of hardcoded Qt5 plugin
lists. Linux additionally needs OpenGL/X11, fontconfig and zlib development files.

The platform scripts and CI matrix have been updated for Qt6.10.1. **Windows/Linux
builds and packages have not been executed in this macOS migration.** Their plugin,
runtime-dependency and packaging checks remain separate roadmap work, including
Linux Qt deployment/package dependencies. Do not interpret the prepared CI
configuration as validated release support on those platforms.
