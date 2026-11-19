# A development executable links against the installed Qt. Leftover deployed
# plugins load the bundled Qt as well, causing duplicate Objective-C classes
# and a failure to initialize Cocoa. qt.conf also keeps plugin lookup local.
get_filename_component(TB_APP_BUNDLE "${TB_APP_BUNDLE}" REALPATH)
if(NOT TB_APP_BUNDLE MATCHES "\\.app$" OR NOT IS_DIRECTORY "${TB_APP_BUNDLE}/Contents/MacOS")
    message(FATAL_ERROR "Expected a generated macOS app bundle: '${TB_APP_BUNDLE}'")
endif()

set(removed_output FALSE)
foreach(deployed_path
        Contents/Frameworks
        Contents/PlugIns
        Contents/Resources/qt.conf
        Contents/_CodeSignature)
    set(path "${TB_APP_BUNDLE}/${deployed_path}")
    if(EXISTS "${path}" OR IS_SYMLINK "${path}")
        file(REMOVE_RECURSE "${path}")
        if(EXISTS "${path}" OR IS_SYMLINK "${path}")
            message(FATAL_ERROR "Could not remove old Qt deployment output: '${path}'")
        endif()
        set(removed_output TRUE)
    endif()
endforeach()
if(removed_output)
    message(STATUS "Removed old Qt deployment output from development bundle '${TB_APP_BUNDLE}'")
endif()
