# Keep compiler results reusable across build directories. Qt and the compiler
# still need to be installed separately; CPM caches sources, not object files.
set(TB_CCACHE_DIR "${CMAKE_SOURCE_DIR}/.cache/ccache" CACHE PATH "Compiler cache directory")
if(TB_ENABLE_CCACHE)
    find_program(CCACHE_PATH NAMES ccache)
    if(CCACHE_PATH)
        message(STATUS "Using ccache: ${CCACHE_PATH} (${TB_CCACHE_DIR})")
        set(_tb_compiler_launcher
            "${CMAKE_COMMAND}" -E env
            "CCACHE_DIR=${TB_CCACHE_DIR}"
            "CCACHE_BASEDIR=${CMAKE_SOURCE_DIR}"
            "${CCACHE_PATH}")
        set(CMAKE_C_COMPILER_LAUNCHER "${_tb_compiler_launcher}")
        set(CMAKE_CXX_COMPILER_LAUNCHER "${_tb_compiler_launcher}")
        if(APPLE)
            set(CMAKE_OBJCXX_COMPILER_LAUNCHER "${_tb_compiler_launcher}")
        endif()
        unset(_tb_compiler_launcher)
    else()
        message(STATUS "ccache not found; continuing without a compiler cache")
    endif()
endif()
