function(set_project_warnings target)
    target_compile_options(${target} PRIVATE
            $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/W4 /WX /permissive- /Zc:__cplusplus>
            $<$<COMPILE_LANG_AND_ID:CXX,Clang,GNU>:-Wall -Wextra -Wpedantic -Werror>
    )
endfunction()

function(set_project_settings target)
    target_compile_options(${target} PRIVATE
            $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/Zc:preprocessor>
    )
    target_compile_definitions(${target} PRIVATE
            $<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN NOMINMAX>
    )
endfunction()