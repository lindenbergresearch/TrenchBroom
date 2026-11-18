# tinyxml2 11.0.0; immutable source plus SHA-256 verification.
CPMAddPackage(
    NAME tinyxml2
    VERSION 11.0.0
    URL "https://codeload.github.com/leethomason/tinyxml2/tar.gz/9148bdf719e997d1f474be6bcc7943881046dba1"
    URL_HASH "SHA256=d0bcc7bf4e3e8ed7d83156e681ab9cb72bd636e5b2bb61d6f7199b32c007707f"
    EXCLUDE_FROM_ALL YES
    SYSTEM YES
    OPTIONS
        "tinyxml2_BUILD_TESTING OFF"
        "tinyxml2_INSTALL OFF"
)
suppress_dependency_warnings(tinyxml2)
