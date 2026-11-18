# Qt 6 splits OpenGL into separate modules and has new Multimedia backends.
# Let Qt choose the matching DLLs/plugins instead of maintaining Qt 5 lists.
function(tb_deploy_windows_qt target)
    if(NOT WIN32)
        return()
    endif()
    if(TARGET Qt6::windeployqt)
        set(deployer "$<TARGET_FILE:Qt6::windeployqt>")
    else()
        get_target_property(qmake_path Qt6::qmake IMPORTED_LOCATION)
        get_filename_component(qt_bin_dir "${qmake_path}" DIRECTORY)
        find_program(TB_WINDEPLOYQT_PATH NAMES windeployqt windeployqt6
            HINTS "${qt_bin_dir}" NO_DEFAULT_PATH REQUIRED)
        set(deployer "${TB_WINDEPLOYQT_PATH}")
    endif()
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${deployer}" --no-translations "$<TARGET_FILE:${target}>"
        VERBATIM)
endfunction()
