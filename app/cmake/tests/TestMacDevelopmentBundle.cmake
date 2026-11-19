if(NOT TB_TEST_ROOT)
    message(FATAL_ERROR "TB_TEST_ROOT must name a temporary test directory")
endif()
set(prepare "${CMAKE_CURRENT_LIST_DIR}/../PrepareMacDevelopmentBundle.cmake")
set(bundle "${TB_TEST_ROOT}/Development Test.app")
file(REMOVE_RECURSE "${TB_TEST_ROOT}")
file(MAKE_DIRECTORY "${bundle}/Contents/MacOS" "${bundle}/Contents/Resources/fonts"
                    "${bundle}/Contents/Frameworks/QtCore.framework"
                    "${bundle}/Contents/PlugIns/platforms" "${bundle}/Contents/_CodeSignature")
file(WRITE "${bundle}/Contents/MacOS/Test" "executable")
file(WRITE "${bundle}/Contents/Info.plist" "bundle metadata")
file(WRITE "${bundle}/Contents/Resources/fonts/Test.ttf" "font resource")
file(WRITE "${bundle}/Contents/Frameworks/QtCore.framework/QtCore" "deployed framework")
file(WRITE "${bundle}/Contents/PlugIns/platforms/libqcocoa.dylib" "deployed plugin")
file(WRITE "${bundle}/Contents/Resources/qt.conf" "[Paths]\nPlugins=PlugIns\n")
file(WRITE "${bundle}/Contents/_CodeSignature/CodeResources" "old resource seal")

function(prepare_bundle)
    execute_process(COMMAND "${CMAKE_COMMAND}"
                            "-DTB_APP_BUNDLE=${bundle}/Contents/MacOS/../.." -P "${prepare}"
                    COMMAND_ERROR_IS_FATAL ANY)
endfunction()
prepare_bundle()
prepare_bundle() # Already clean is a no-op.
foreach(path Contents/Frameworks Contents/PlugIns Contents/Resources/qt.conf Contents/_CodeSignature)
    if(EXISTS "${bundle}/${path}")
        message(FATAL_ERROR "Stale deployment output remains: ${path}")
    endif()
endforeach()
foreach(path Contents/MacOS/Test Contents/Info.plist Contents/Resources/fonts/Test.ttf)
    if(NOT EXISTS "${bundle}/${path}")
        message(FATAL_ERROR "Development cleanup removed an application resource: ${path}")
    endif()
endforeach()

if(UNIX)
    # A generated deployment entry may be a link; never remove its destination.
    file(MAKE_DIRECTORY "${TB_TEST_ROOT}/outside")
    file(WRITE "${TB_TEST_ROOT}/outside/sentinel" "preserve")
    file(CREATE_LINK "${TB_TEST_ROOT}/outside" "${bundle}/Contents/Frameworks" SYMBOLIC)
    prepare_bundle()
    if(IS_SYMLINK "${bundle}/Contents/Frameworks" OR NOT EXISTS "${TB_TEST_ROOT}/outside/sentinel")
        message(FATAL_ERROR "Cleanup followed or retained a deployment symlink")
    endif()
endif()

# Reject an accidental non-bundle target before deleting anything.
file(MAKE_DIRECTORY "${TB_TEST_ROOT}/not-a-bundle/Contents/MacOS"
                    "${TB_TEST_ROOT}/not-a-bundle/Contents/Frameworks")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DTB_APP_BUNDLE=${TB_TEST_ROOT}/not-a-bundle"
                        -P "${prepare}"
                RESULT_VARIABLE invalid_result OUTPUT_QUIET ERROR_QUIET)
if(invalid_result EQUAL 0 OR NOT IS_DIRECTORY "${TB_TEST_ROOT}/not-a-bundle/Contents/Frameworks")
    message(FATAL_ERROR "Cleanup accepted a non-bundle directory")
endif()
file(REMOVE_RECURSE "${TB_TEST_ROOT}")
