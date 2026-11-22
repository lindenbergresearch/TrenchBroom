add_library(fontstash-rasterizer STATIC src/GlyphRasterizer.cpp)
target_include_directories(fontstash-rasterizer PUBLIC include)
target_include_directories(fontstash-rasterizer SYSTEM PRIVATE vendor)
target_link_libraries(fontstash-rasterizer PUBLIC Qt6::Gui)
