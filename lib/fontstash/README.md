# FontStash glyph rasterizer

The rasterizer comes from the NanoVG fork used by PlugData
(`Libraries/nanovg`, revision `8249b37`). The vendored `stb_truetype.h`
adds an entry point for a caller-provided outline, using the same signed
distance calculation as `stbtt_GetGlyphSDF`. See the license in the header.

`GlyphRasterizer.cpp` adapts the rasterization settings and trigonometric
approximations from `fontstash.h` and the four-sample SDF coverage function
from `nanovg_mtl_shaders.metal`. These adapted portions are covered by
`vendor/LICENSE.txt`. Qt supplies the shaped glyphs and their outlines.
