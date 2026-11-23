# Focused, maintained migration suite. The older common/test target still
# contains unrelated API drift; do not silently equate this with the full suite.
set(regression_sources
    Main.cpp
    tst_Dependencies.cpp
    tst_Qt6.cpp
    tst_SdfText.cpp
    ../test/src/View/Input/tst_InputEvent.cpp
    ../test/src/View/Prefabs/tst_Prefab.cpp
    ../test/src/TestPreferenceManager.cpp
    ../test/src/View/Document/tst_BulkSelection.cpp
    ../test/src/IO/tst_ArchiveNames.cpp
    ../test/src/IO/tst_ParserStatus.cpp
    ../test/src/IO/tst_DiskCache.cpp
    ../test/src/IO/tst_GameFileSystemCache.cpp
    ../test/src/Renderer/tst_Annotations.cpp
    ../test/src/IO/tst_VectorIcon.cpp
    ../test/src/Assets/tst_EntityModelAnimation.cpp
    ../test/src/View/Theme/tst_Theme.cpp
    ../test/src/View/Widgets/tst_SliderWithLabel.cpp
    ../test/src/View/Actions/tst_EditorConfiguration.cpp
    ../test/src/View/Widgets/tst_SearchableComboBox.cpp
    ../test/src/View/Preferences/tst_IconSelectionDialog.cpp
    ../test/src/View/Preferences/tst_FontUtils.cpp
    ../test/src/View/Inspector/Entity/tst_EntityExtensions.cpp
    ../test/src/View/Inspector/Layer/tst_LayerTree.cpp
    ../test/src/View/Inspector/ViewSettings/tst_ViewEditor.cpp
    ../test/src/View/Inspector/tst_Inspector.cpp
    ../test/src/View/Actions/tst_ContextMenuActions.cpp
    ../test/src/View/Inspector/tst_History.cpp
    ../test/src/View/App/tst_MapLoadDiagnostics.cpp
    ../test/src/View/MapView/tst_ViewState.cpp
    ../test/src/View/Commands/tst_CommandProcessor.cpp
    ../test/src/View/Widgets/tst_StylesheetControls.cpp
    ../test/src/View/Compilation/tst_TextOutputAdapter.cpp
    ../test/src/View/Inspector/Properties/tst_WadProperty.cpp
)
if(APPLE)
    list(APPEND regression_sources tst_MacWindowStyle.mm)
    # This native test uses ARC; do not reuse the C++ suite's non-ARC PCH.
    set_source_files_properties(tst_MacWindowStyle.mm PROPERTIES
        COMPILE_OPTIONS "-fobjc-arc" SKIP_PRECOMPILE_HEADERS ON)
endif()
add_executable(tb-regression-tests EXCLUDE_FROM_ALL ${regression_sources})
target_link_libraries(tb-regression-tests PRIVATE common Catch2::Catch2 Qt6::Test)
target_include_directories(tb-regression-tests PRIVATE ../test/src)
target_compile_definitions(tb-regression-tests PRIVATE
    CATCH_CONFIG_ENABLE_ALL_STRINGMAKERS=1
    TB_TEST_SOURCE_ROOT="${CMAKE_SOURCE_DIR}")
set_target_properties(tb-regression-tests PROPERTIES
    AUTOMOC ON
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/regression")
set_compiler_config(tb-regression-tests)

# Resource lookup follows the same paths as the editor. Refresh them even if
# only a QSS, icon or game configuration changed since the last link.
add_custom_target(tb-regression-resources
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:tb-regression-tests>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        "${APP_RESOURCE_DIR}/graphics/images" "$<TARGET_FILE_DIR:tb-regression-tests>/images")
foreach(resource cursor defaults fonts games shader stylesheets)
    add_custom_command(TARGET tb-regression-resources POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            "${APP_RESOURCE_DIR}/${resource}" "$<TARGET_FILE_DIR:tb-regression-tests>/${resource}")
endforeach()
add_dependencies(tb-regression-tests tb-regression-resources)
tb_deploy_windows_qt(tb-regression-tests)
if(WIN32 AND TARGET Qt6::QOffscreenIntegrationPlugin)
    add_custom_command(TARGET tb-regression-tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:tb-regression-tests>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Qt6::QOffscreenIntegrationPlugin>" "$<TARGET_FILE_DIR:tb-regression-tests>/platforms")
endif()
add_test(NAME tb-regression-tests COMMAND tb-regression-tests)
set_tests_properties(tb-regression-tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 300)
