# Force bundled minizip: Homebrew's minizip places unzip.h in a different
# directory. Assimp uses system zlib on macOS/Linux.
# assimp 6.0.4; immutable source plus SHA-256 verification.
CPMAddPackage(
    NAME assimp
    VERSION 6.0.4
    URL "https://codeload.github.com/assimp/assimp/tar.gz/e0b52347c6e52de2827ec957a9ebf00ce3c54f79"
    URL_HASH "SHA256=00e4bf0b9d9d8e9b346fa3dd859c3ce5f3438451822d5933c07f65a76fc04745"
    EXCLUDE_FROM_ALL YES
    SYSTEM YES
    OPTIONS
        "ASSIMP_NO_EXPORT ON"
        # Assimp otherwise replaces the global compile/link launchers, bypassing
        # our cache directory and even wrapping archive/link commands in ccache.
        "ASSIMP_BUILD_USE_CCACHE OFF"
        "ASSIMP_BUILD_TESTS OFF"
        "ASSIMP_BUILD_ASSIMP_TOOLS OFF"
        "ASSIMP_BUILD_SAMPLES OFF"
        "ASSIMP_WARNINGS_AS_ERRORS OFF"
        "ASSIMP_INSTALL OFF"
        "ASSIMP_INSTALL_PDB OFF"
        "ASSIMP_BUILD_MINIZIP ON"
)
suppress_dependency_warnings(assimp)
