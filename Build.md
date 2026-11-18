# Building this TrenchBroom fork

The first Qt 6 migration milestone uses **CPM.cmake with Qt 5.15**. Qt 6 is the
next milestone; installing Qt 6 alone is not sufficient to build this revision.
The migration status and estimates are in [Qt6-CPM-Roadmap.md](doc/Qt6-CPM-Roadmap.md).

## Prerequisites

- A C++20 compiler and its platform SDK.
- CMake **3.28 or newer**, Ninja, Git and Pandoc.
- Qt **5.15** with Core, Widgets, SVG, Multimedia, Network and Test.
- OpenGL development libraries and zlib (provided by the macOS SDK).
- Optional: `ccache` for reusing compiler results.

Qt and system OpenGL are installed separately. CPM downloads and builds Assimp,
Catch2, fmt, FreeImage, FreeType, GLEW, miniz and tinyxml2 from verified source
archives. Exact versions, source URLs and SHA-256 hashes are in
[`cmake/dependencies/`](cmake/dependencies). Catch2 intentionally remains on
version 2. FreeImage uses the CMake-enabled fork used by original TrenchBroom.
Boost is not required by this fork's source code.

The old vcpkg submodule, manifest and overlays remain as a migration reference.
They are **not used by the new build**; there is no need to initialize that
submodule. Use a **fresh build directory**. Reusing a directory configured with
the former vcpkg toolchain is rejected before CMake loads the toolchain.

## macOS (validated on Apple Silicon)

Install Xcode / Command Line Tools, and the build tools, for example:

```sh
brew install cmake ninja pandoc qt@5 ccache
```

Configure, build and run the migration regression suite from the repository root:

```sh
cmake --preset qt5-cpm \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt@5)" \
  -DTB_RUN_MACDEPLOYQT=ON
cmake --build --preset qt5-cpm --parallel 8
ctest --preset qt5-cpm
open cmake-build-cpm-qt5/app/TrenchBroom.app
```

`TB_RUN_MACDEPLOYQT=ON` copies Qt frameworks and plugins into the app bundle,
then signs and verifies the generated bundle. The default
`TB_MAC_CODESIGN_IDENTITY=-` uses a local ad-hoc signature without a certificate.
Finder metadata is removed only from the generated bundle before signing.
Deployment errors are treated as build failures, even when Qt 5's deployment
tool returns a zero exit code.

For faster development builds it can be omitted; the resulting app then uses
the installed Qt. The preset does not hardcode a Homebrew path, architecture or
SDK. Set `CMAKE_OSX_DEPLOYMENT_TARGET` / `CMAKE_OSX_ARCHITECTURES` explicitly when
needed; CPM libraries are built with the same settings as the editor.

The equivalent scripted build is:

```sh
TB_PACKAGE=false ./CI-macos.sh
```

`CI-macos.sh` also accepts `TB_BUILD_DIR`, `TB_QT_PREFIX`, `TB_DEBUG_BUILD=true`
and `CMAKE_BUILD_PARALLEL_LEVEL`. Packaging is enabled by default for release
builds. These commands build the editor and focused regression suite rather
than the unmaintained older test/benchmark targets.

## Caches and repeat builds

Two independent caches are used by default:

| Cache | Default directory | Purpose |
| --- | --- | --- |
| CPM | `.cache/cpm` | Downloaded/extracted, pinned dependency sources |
| ccache | `.cache/ccache` | Compiler results, when `ccache` is installed |

Both directories are ignored by Git. They survive removal of a build directory.
A new build still configures and links its own targets; source caching alone
does not reuse compiled libraries. `ccache` validates compiler options, headers
and SDK inputs before returning a cached result. CMake's precompiled-header
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
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt@5)" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
```

Do not edit files inside the shared source cache. Changes to dependencies should
be explicit patches or new version/hash pins, so subsequent builds are repeatable.

## Regression targets

`tb-regression-tests` is a checked-in, focused migration suite, including existing
regressions for themes, fonts/icons, editor actions/toolbars, Inspector docking,
Layer selection, WAD properties, History/Undo, view state and archive caches.
Additional checks exercise the real image/model/font loaders after the CPM change.
The runner uses isolated temporary settings and Qt's offscreen platform through
CTest. It does not change the editor's saved settings.

To run a subset:

```sh
QT_QPA_PLATFORM=offscreen \
  cmake-build-cpm-qt5/regression/tb-regression-tests '[dependencies]'
```

The historical `common/test` and benchmark targets remain disabled: several of
their files still use older project APIs. This suite is not a claim that all old
tests have been ported. Native Cocoa/OpenGL checks are separate from offscreen
checks and remain necessary for the later Qt 6 renderer migration.

## Windows and Linux

The CMake dependency definitions and static library targets are shared across
platforms. A separately installed Qt 5.15 prefix can be supplied with
`CMAKE_PREFIX_PATH` when using the `qt5-cpm` preset (Ninja requires the appropriate
compiler environment). Linux additionally needs the OpenGL/X11 and zlib development
packages, and Qt Multimedia development files.

Windows/Linux compilation, packaging and the older CI scripts have not been
validated in this macOS migration milestone. Their full update and platform
checks are separate roadmap work; do not use this milestone as a release claim
for those platforms. The CI cache step already uses CPM sources / compiler results
instead of bootstrapping vcpkg.
