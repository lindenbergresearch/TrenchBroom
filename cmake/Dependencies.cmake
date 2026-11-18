# Same dependency mechanism as upstream TrenchBroom. Qt is installed separately;
# URL hashes and versions for the native libraries live in dependencies/.
if(NOT DEFINED CPM_SOURCE_CACHE AND NOT DEFINED ENV{CPM_SOURCE_CACHE})
    set(CPM_SOURCE_CACHE "${CMAKE_SOURCE_DIR}/.cache/cpm" CACHE PATH "CPM source cache directory")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/CPM.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/dependencies/CompilerConfig.cmake")

# Several upstream projects have generic BUILD_TESTS / BUILD_SHARED_LIBS options.
# Scope their defaults so they do not change the editor's build configuration.
function(tb_configure_dependencies)
    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_TESTING OFF)
    set(BUILD_TESTS OFF)
    # Some bundled codecs still request older CMake policy versions.
    set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
    foreach(package assimp catch2 fmt freeimage freetype glew miniz tinyxml2)
        include("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/dependencies/${package}.cmake")
    endforeach()
endfunction()
tb_configure_dependencies()
