# macdeployqt 5 can report deployment/signing errors but still return zero.
# Check its diagnostics, then sign and verify the complete generated bundle.
get_filename_component(TB_APP_BUNDLE "${TB_APP_BUNDLE}" REALPATH)
set(deploy_options -always-overwrite)
if(TB_MACDEPLOYQT_LIBRARY_PATH)
    list(APPEND deploy_options "-libpath=${TB_MACDEPLOYQT_LIBRARY_PATH}")
endif()
execute_process(
    COMMAND "${TB_MACDEPLOYQT}" "${TB_APP_BUNDLE}" ${deploy_options}
    RESULT_VARIABLE deploy_result OUTPUT_VARIABLE deploy_output ERROR_VARIABLE deploy_error)
if(NOT deploy_result EQUAL 0 OR deploy_output MATCHES "ERROR:" OR deploy_error MATCHES "ERROR:")
    message(FATAL_ERROR "Qt deployment failed (${deploy_result}):\n${deploy_output}\n${deploy_error}")
endif()

# Qt 5 misses some transitive @rpath dependencies of image plugins (e.g.
# libsharpyuv used by WebP). CMake resolves those from the same library path
# and rewrites them to bundle-relative paths before the signature is created.
include(BundleUtilities)
file(GLOB_RECURSE bundle_plugins "${TB_APP_BUNDLE}/Contents/PlugIns/*.dylib")
fixup_bundle("${TB_APP_BUNDLE}" "${bundle_plugins}" "${TB_MACDEPLOYQT_LIBRARY_PATH}")

# Copied font/image resources can carry Finder metadata that codesign rejects.
# Only clean the generated bundle, never the source resources or installed Qt.
# Some deployed fonts are read-only. Do not follow symbolic links outside the
# bundle when adjusting permissions or removing extended attributes.
execute_process(COMMAND /bin/chmod -R -P u+w "${TB_APP_BUNDLE}" COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND /usr/bin/xattr -crs "${TB_APP_BUNDLE}" COMMAND_ERROR_IS_FATAL ANY)
# Deployment changes Mach-O load commands, invalidating the arm64 linker's
# automatic signature. A local ad-hoc signature requires no signing certificate.
execute_process(COMMAND /usr/bin/codesign --force --deep --sign "${TB_CODESIGN_IDENTITY}" "${TB_APP_BUNDLE}"
    COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND /usr/bin/codesign --verify --deep --strict "${TB_APP_BUNDLE}"
    COMMAND_ERROR_IS_FATAL ANY)
