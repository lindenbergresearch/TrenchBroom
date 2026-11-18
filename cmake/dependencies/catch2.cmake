# Keep Catch2 2: upgrading the test API is a separate migration.
# Catch2 2.13.8; immutable source plus SHA-256 verification.
CPMAddPackage(
    NAME Catch2
    VERSION 2.13.8
    URL "https://codeload.github.com/catchorg/Catch2/tar.gz/refs/tags/v2.13.8"
    URL_HASH "SHA256=b9b592bd743c09f13ee4bf35fc30eeee2748963184f6bea836b146e6cc2a585a"
    EXCLUDE_FROM_ALL YES
    SYSTEM YES
    OPTIONS
        "CATCH_BUILD_TESTING OFF"
        "CATCH_BUILD_EXAMPLES OFF"
        "CATCH_BUILD_EXTRA_TESTS OFF"
        "CATCH_INSTALL_DOCS OFF"
        "CATCH_INSTALL_HELPERS OFF"
)
