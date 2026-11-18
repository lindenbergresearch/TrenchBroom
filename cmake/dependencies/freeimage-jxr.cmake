# The fork's BUILD_JXR source list is incomplete. Use the codec sources from
# its release Makefile explicitly, preserving the former FreeImage feature set.
set(_tb_jxr_sources
    "${FreeImage_SOURCE_DIR}/Source/FreeImage/PluginJXR.cpp"
    "${FreeImage_SOURCE_DIR}/Source/LibJXR/image/decode/JXRTranscode.c"
    "${FreeImage_SOURCE_DIR}/Source/LibJXR/image/decode/decode.c"
    "${FreeImage_SOURCE_DIR}/Source/LibJXR/image/decode/postprocess.c"
    "${FreeImage_SOURCE_DIR}/Source/LibJXR/image/decode/segdec.c"
    "${FreeImage_SOURCE_DIR}/Source/LibJXR/image/decode/strInvTransform.c"
    "${FreeImage_SOURCE_DIR}/Source/LibJXR/image/decode/strPredQuantDec.c"
    "${FreeImage_SOURCE_DIR}/Source/LibJXR/image/decode/strdec.c"
    "${FreeImage_SOURCE_DIR}/Source/LibJXR/image/decode/strdec_x86.c"
    "${FreeImage_SOURCE_DIR}/Source/LibJXR/image/encode/encode.c"
    "${FreeImage_SOURCE_DIR}/Source/LibJXR/image/encode/segenc.c"
    "${FreeImage_SOURCE_DIR}/Source/LibJXR/image/encode/strFwdTransform.c"
    "${FreeImage_SOURCE_DIR}/Source/LibJXR/image/encode/strPredQuantEnc.c"
    "${FreeImage_SOURCE_DIR}/Source/LibJXR/image/encode/strenc.c"
    "${FreeImage_SOURCE_DIR}/Source/LibJXR/image/encode/strenc_x86.c"
    "${FreeImage_SOURCE_DIR}/Source/LibJXR/image/sys/adapthuff.c"
    "${FreeImage_SOURCE_DIR}/Source/LibJXR/image/sys/image.c"
    "${FreeImage_SOURCE_DIR}/Source/LibJXR/image/sys/strPredQuant.c"
    "${FreeImage_SOURCE_DIR}/Source/LibJXR/image/sys/strTransform.c"
    "${FreeImage_SOURCE_DIR}/Source/LibJXR/image/sys/strcodec.c"
    "${FreeImage_SOURCE_DIR}/Source/LibJXR/jxrgluelib/JXRGlue.c"
    "${FreeImage_SOURCE_DIR}/Source/LibJXR/jxrgluelib/JXRGlueJxr.c"
    "${FreeImage_SOURCE_DIR}/Source/LibJXR/jxrgluelib/JXRGluePFC.c"
    "${FreeImage_SOURCE_DIR}/Source/LibJXR/jxrgluelib/JXRMeta.c"
)
target_sources(FreeImage PRIVATE ${_tb_jxr_sources})
if(MSVC)
    set(_tb_jxr_include "/FI${CMAKE_CURRENT_LIST_DIR}/FreeImageJxrCompat.h")
else()
    set(_tb_jxr_include "-include;${CMAKE_CURRENT_LIST_DIR}/FreeImageJxrCompat.h")
endif()
set_property(SOURCE ${_tb_jxr_sources} TARGET_DIRECTORY FreeImage APPEND
    PROPERTY COMPILE_OPTIONS "${_tb_jxr_include}")
target_include_directories(FreeImage PRIVATE
    "${FreeImage_SOURCE_DIR}/Source/LibJXR"
    "${FreeImage_SOURCE_DIR}/Source/LibJXR/image/sys"
    "${FreeImage_SOURCE_DIR}/Source/LibJXR/common/include"
    "${FreeImage_SOURCE_DIR}/Source/LibJXR/jxrgluelib")
target_compile_definitions(FreeImage PRIVATE INCLUDE_LIB_JXR __ANSI__)
