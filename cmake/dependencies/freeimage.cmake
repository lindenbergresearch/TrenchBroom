# CMake-enabled fork used by upstream TrenchBroom, with bundled codecs.
# FreeImage 3.19.11; immutable source plus SHA-256 verification.
CPMAddPackage(
    NAME FreeImage
    VERSION 3.19.11
    URL "https://codeload.github.com/danoli3/FreeImage/tar.gz/625117b48e18a82f3f68e9be3514ae584ebfcf7b"
    URL_HASH "SHA256=ebbb3b5a72f77d35dc97a12aab4593414b372f8cc55be6a25a7df942a6faad55"
    EXCLUDE_FROM_ALL YES
    SYSTEM YES
    OPTIONS
        "BUILD_JXR OFF"
        "BUILD_TESTS OFF"
        "BUILD_ZLIB ON"
        "BUILD_LIBPNG ON"
        "BUILD_LIBTIFF ON"
        "BUILD_WEBP ON"
        "BUILD_LIBRAWLITE ON"
        "BUILD_OPENEXR ON"
)
include("${CMAKE_CURRENT_LIST_DIR}/freeimage-jxr.cmake")
suppress_dependency_warnings(FreeImage)
# Bundled OpenEXR/LibRaw still use pre-C++17 APIs. Only this C API library
# needs the older language mode; editor sources remain on C++20.
set_target_properties(FreeImage PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
# The upstream target omits these usage requirements for static consumers.
target_include_directories(FreeImage SYSTEM INTERFACE "$<BUILD_INTERFACE:${FreeImage_SOURCE_DIR}/Source>")
target_compile_definitions(FreeImage INTERFACE FREEIMAGE_LIB)
add_library(freeimage::FreeImage ALIAS FreeImage)
