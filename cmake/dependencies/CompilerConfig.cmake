# Third-party sources retain their own compiler settings, without the editor's
# warning policy. Sanitizer options are inherited from the parent directory.
function(suppress_dependency_warnings target)
    get_target_property(target_type "${target}" TYPE)
    if(NOT target_type STREQUAL "INTERFACE_LIBRARY")
        target_compile_options("${target}" PRIVATE
            $<$<COMPILE_LANG_AND_ID:C,AppleClang,Clang,GNU>:-w>
            $<$<COMPILE_LANG_AND_ID:CXX,AppleClang,Clang,GNU>:-w>
            $<$<COMPILE_LANG_AND_ID:C,MSVC>:/w>
            $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/w>)
    endif()
endfunction()
