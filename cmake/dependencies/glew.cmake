# Use the release archive: Git snapshots lack the generated GL headers/source.
# GLEW remains in use until a separate renderer/GL-loader migration.
# glew 2.3.1; immutable source plus SHA-256 verification.
CPMAddPackage(
    NAME glew
    VERSION 2.3.1
    URL "https://github.com/nigels-com/glew/releases/download/glew-2.3.1/glew-2.3.1.tgz"
    URL_HASH "SHA256=b64790f94b926acd7e8f84c5d6000a86cb43967bd1e688b03089079799c9e889"
    EXCLUDE_FROM_ALL YES
    SYSTEM YES
    SOURCE_SUBDIR build/cmake
    OPTIONS
        "BUILD_UTILS OFF"
        "BUILD_SHARED_LIBS OFF"
        "GLEW_CUSTOM_OUTPUT_DIRS ON"
)
suppress_dependency_warnings(glew_s)
# The upstream build interface does not publish its header directory.
target_include_directories(glew_s SYSTEM INTERFACE "$<BUILD_INTERFACE:${glew_SOURCE_DIR}/include>")
add_library(GLEW::GLEW ALIAS glew_s)
