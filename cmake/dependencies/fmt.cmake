# fmt 12.2.0; immutable source plus SHA-256 verification.
CPMAddPackage(
    NAME fmt
    VERSION 12.2.0
    URL "https://codeload.github.com/fmtlib/fmt/tar.gz/1be298e1bd68957e4cd352e1f676f00e07dcfb57"
    URL_HASH "SHA256=d041f836d9b2a3c1c0fe3cce81ffe6a4a257028faa2f4af0dd20d7e58b493766"
    EXCLUDE_FROM_ALL YES
    SYSTEM YES
    OPTIONS
        "FMT_DOC OFF"
        "FMT_TEST OFF"
        "FMT_INSTALL OFF"
)
suppress_dependency_warnings(fmt)
