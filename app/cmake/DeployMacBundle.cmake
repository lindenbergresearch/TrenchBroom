# macdeployqt can report deployment/signing errors but still return zero.
# Check its diagnostics, then sign and verify the complete generated bundle.
get_filename_component(TB_APP_BUNDLE "${TB_APP_BUNDLE}" REALPATH)
# The executable has just been relinked against the installed Qt. Do not let
# dependencies rewritten during an earlier deployment influence this run.
# These two directories contain generated deployment output exclusively.
file(REMOVE_RECURSE "${TB_APP_BUNDLE}/Contents/Frameworks" "${TB_APP_BUNDLE}/Contents/PlugIns")
set(deploy_options -always-overwrite)
if(TB_MACDEPLOYQT_LIBRARY_PATH)
    list(APPEND deploy_options "-libpath=${TB_MACDEPLOYQT_LIBRARY_PATH}")
    # Homebrew installs Qt 6 modules in separate prefixes. macdeployqt does
    # not apply -libpath to every framework @rpath used by optional plugins.
    # macdeployqt only propagates rpaths actually used to resolve a dependency.
    # Homebrew links frameworks with absolute install names, so merely adding
    # an rpath is insufficient. Normalize these Qt imports before deployment.
    execute_process(COMMAND /usr/bin/otool -L "${TB_APP_EXECUTABLE}"
        OUTPUT_VARIABLE app_imports COMMAND_ERROR_IS_FATAL ANY)
    string(REPLACE "\n" ";" app_import_lines "${app_imports}")
    foreach(import_line IN LISTS app_import_lines)
        string(REGEX REPLACE "^[ \t]+| \\(compatibility.*$" "" import_path "${import_line}")
        if(IS_ABSOLUTE "${import_path}" AND import_path MATCHES "/(Qt[^/]+\\.framework/.*)$")
            execute_process(COMMAND /usr/bin/install_name_tool -change
                "${import_path}" "@rpath/${CMAKE_MATCH_1}" "${TB_APP_EXECUTABLE}"
                COMMAND_ERROR_IS_FATAL ANY)
        endif()
    endforeach()
    # The temporary search path is removed once all imports are bundle-relative.
    execute_process(COMMAND /usr/bin/otool -l "${TB_APP_EXECUTABLE}"
        OUTPUT_VARIABLE app_load_commands COMMAND_ERROR_IS_FATAL ANY)
    string(FIND "${app_load_commands}" "path ${TB_MACDEPLOYQT_LIBRARY_PATH} (offset" library_path_index)
    if(library_path_index LESS 0)
        execute_process(COMMAND /usr/bin/install_name_tool -add_rpath
            "${TB_MACDEPLOYQT_LIBRARY_PATH}" "${TB_APP_EXECUTABLE}" COMMAND_ERROR_IS_FATAL ANY)
        set(added_library_rpath TRUE)
    endif()
endif()
execute_process(
    COMMAND "${TB_MACDEPLOYQT}" "${TB_APP_BUNDLE}" ${deploy_options}
    RESULT_VARIABLE deploy_result OUTPUT_VARIABLE deploy_output ERROR_VARIABLE deploy_error)
if(NOT deploy_result EQUAL 0 OR deploy_output MATCHES "ERROR:" OR deploy_error MATCHES "ERROR:")
    message(FATAL_ERROR "Qt deployment failed (${deploy_result}):\n${deploy_output}\n${deploy_error}")
endif()

# macdeployqt misses some transitive @rpath dependencies of image plugins (e.g.
# libsharpyuv used by WebP). CMake resolves those from the same library path
# and rewrites them to bundle-relative paths before the signature is created.
include(BundleUtilities)
file(GLOB_RECURSE bundle_plugins "${TB_APP_BUNDLE}/Contents/PlugIns/*.dylib")
fixup_bundle("${TB_APP_BUNDLE}" "${bundle_plugins}" "${TB_MACDEPLOYQT_LIBRARY_PATH}")

if(added_library_rpath)
    execute_process(COMMAND /usr/bin/otool -l "${TB_APP_EXECUTABLE}"
        OUTPUT_VARIABLE app_load_commands COMMAND_ERROR_IS_FATAL ANY)
    string(FIND "${app_load_commands}" "path ${TB_MACDEPLOYQT_LIBRARY_PATH} (offset" library_path_index)
    if(NOT library_path_index LESS 0)
        execute_process(COMMAND /usr/bin/install_name_tool -delete_rpath
            "${TB_MACDEPLOYQT_LIBRARY_PATH}" "${TB_APP_EXECUTABLE}" COMMAND_ERROR_IS_FATAL ANY)
    endif()
endif()

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
