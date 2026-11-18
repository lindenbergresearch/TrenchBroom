# miniz 3.1.2; immutable source plus SHA-256 verification.
CPMAddPackage(
    NAME miniz
    VERSION 3.1.2
    URL "https://codeload.github.com/richgel999/miniz/tar.gz/77d0dce8627735138c51770d1799a1ef48f2117d"
    URL_HASH "SHA256=baa14349e7c5efb4a493a03af7d2f29d0ef45d87d488567fe5360edb33f8b7ba"
    EXCLUDE_FROM_ALL YES
    SYSTEM YES
    OPTIONS
        "BUILD_EXAMPLES OFF"
        "BUILD_FUZZERS OFF"
        "BUILD_TESTS OFF"
        "INSTALL_PROJECT OFF"
)
suppress_dependency_warnings(miniz)
add_library(miniz::miniz ALIAS miniz)
