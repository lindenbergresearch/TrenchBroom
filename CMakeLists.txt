cmake_minimum_required(VERSION 3.28)

# A vcpkg-configured build directory cannot safely be reused with CPM. Check
# before project() loads the cached toolchain and installs any packages.
if(CMAKE_TOOLCHAIN_FILE MATCHES "(^|[/\\\\])vcpkg([/\\\\]|\\.cmake)")
    message(FATAL_ERROR
        "This build directory uses the old vcpkg toolchain. Configure a fresh "
        "directory with the qt6-cpm preset; see Build.md.")
endif()

# Fix warning with Ninja and cotire (see https://github.com/sakra/cotire/issues/81)
if(POLICY CMP0058)
    cmake_policy(SET CMP0058 NEW)
endif()

# Instruct cmake not to set default warning levels for MSVC projects (cmake 3.15 or higher)
if(POLICY CMP0092)
    cmake_policy(SET CMP0092 NEW)
endif()

if(POLICY CMP0063)
    cmake_policy(SET CMP0063 NEW)
endif()

# Qt 5 and Qt 6 autogen files and plugin paths must stay in separate caches.
if(DEFINED Qt5Core_DIR AND NOT DEFINED Qt6Core_DIR)
    message(FATAL_ERROR "This is a Qt5 build directory. Use a fresh directory with the qt6-cpm preset; see Build.md.")
endif()

set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN TRUE)

# Enable the source_group command for creating IDE folders
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Set TrenchBroom item as default single startup project (NOTE: CMake 3.6 and newer)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT TrenchBroom)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# Default editor deployment target. The installed Qt binaries may require a
# newer OS; use a matching Qt SDK when distributing to older macOS versions.
if(CMAKE_HOST_APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0" CACHE STRING "Minimum macOS deployment version")
endif()

# Using C and CXX because GLEW is C
project(TrenchBroom LANGUAGES C CXX)
if(APPLE)
    enable_language(OBJCXX)
endif()

option(TB_ENABLE_CCACHE "Use ccache when available" ON)
option(TB_BUILD_REGRESSION_TESTS "Provide the focused migration regression target" ON)
option(TB_RUN_MACDEPLOYQT "Bundle Qt frameworks and plugins on macOS" OFF)
if(APPLE)
    set(TB_MAC_CODESIGN_IDENTITY "-" CACHE STRING "macOS signing identity; - signs local builds ad hoc")
endif()
include(cmake/CompilerCache.cmake)

# Compiler detection
set(COMPILER_IS_CLANG FALSE)
set(COMPILER_IS_GNU FALSE)
set(COMPILER_IS_MSVC FALSE)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    set(COMPILER_IS_CLANG TRUE)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(COMPILER_IS_GNU TRUE)
elseif(MSVC EQUAL 1)
    set(COMPILER_IS_MSVC TRUE)
else()
    message(FATAL_ERROR "Unsupported compiler detected.")
endif()

# Request 8MB stack on Windows. (NOTE: /STACK should be used on .exe targets only.)
if(COMPILER_IS_MSVC)
    set(TB_STACK_SIZE 8388608)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /STACK:${TB_STACK_SIZE}")
endif()

# Enable ASAN if possible and requested
if(TB_ENABLE_ASAN)
    message(STATUS "Enabling ASan")

    if(COMPILER_IS_CLANG OR COMPILER_IS_GNU)
        add_compile_options(-fsanitize=address)
        add_link_options(-fsanitize=address)
    else()
        message(WARNING "TB isn't set up to enable ASan for compiler ${CMAKE_CXX_COMPILER_ID}")
    endif()
endif()

include(cmake/Utils.cmake)

# Find Git
find_package(Git)

if(NOT GIT_FOUND)
    message(FATAL_ERROR "Could not find git")
endif()

# Find Pandoc
if(NOT PANDOC_PATH)
    find_program(PANDOC_PATH NAMES "pandoc" DOC "Pandoc program location")

    if(PANDOC_PATH STREQUAL "PANDOC_PATH-NOTFOUND")
        message(FATAL_ERROR "Could not find pandoc")
    else()
        message(STATUS "Found Pandoc: ${PANDOC_PATH}")
    endif()
endif()

include(cmake/Dependencies.cmake)

# Find Qt and OpenGL
find_package(OpenGL REQUIRED)
find_package(Qt6 6.10 COMPONENTS Core Widgets Svg Multimedia Network Test OpenGL OpenGLWidgets REQUIRED)
include(cmake/QtDeployment.cmake)


# The editor and native dependencies use threads.
find_package(Threads)

# Populate version variables using git
get_git_describe("${GIT_EXECUTABLE}" "${CMAKE_SOURCE_DIR}" GIT_DESCRIBE)
get_app_version(GIT_DESCRIBE APP_VERSION_YEAR APP_VERSION_NUMBER)
set(APP_BUILD_TYPE "${CMAKE_BUILD_TYPE}")

# Some global variables used in several targets
set(APP_DIR "${CMAKE_SOURCE_DIR}/app")
set(APP_RESOURCE_DIR "${APP_DIR}/resources")

enable_testing()
add_subdirectory(lib)
add_subdirectory(dump-shortcuts)
add_subdirectory(return-exitCode)
add_subdirectory(common)
add_subdirectory(app)

if(TB_BUILD_REGRESSION_TESTS)
    add_subdirectory(common/regression)
endif()
