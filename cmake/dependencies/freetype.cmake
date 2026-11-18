# Only the TrueType rasterizer is needed; no optional external codecs.
# freetype 2.14.3; immutable source plus SHA-256 verification.
CPMAddPackage(
    NAME freetype
    VERSION 2.14.3
    URL "https://codeload.github.com/freetype/freetype/tar.gz/0a0221a1347e2f1e07c395263540026e9a0aa7c7"
    URL_HASH "SHA256=11cd478953fc1d382f20a233b8e2aed6c31b47cbf0568c4bdb3334bcc1550698"
    EXCLUDE_FROM_ALL YES
    SYSTEM YES
    OPTIONS
        "FT_DISABLE_ZLIB ON"
        "FT_DISABLE_BZIP2 ON"
        "FT_DISABLE_PNG ON"
        "FT_DISABLE_HARFBUZZ ON"
        "FT_DISABLE_BROTLI ON"
        "SKIP_INSTALL_ALL ON"
)
suppress_dependency_warnings(freetype)
