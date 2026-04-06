option(ENABLE_SANITIZERS "Enable ASan + UBSan" OFF)

function(enable_sanitizers target)
    if(NOT ENABLE_SANITIZERS)
        return()
    endif()

    if(MSVC)
        # MSVC only supports ASan; UBSan is not available
        target_compile_options(${target} PRIVATE /fsanitize=address)
        target_link_options(${target} PRIVATE /fsanitize=address)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target} PRIVATE
                -fsanitize=address,undefined
                -fno-omit-frame-pointer
        )
        target_link_options(${target} PRIVATE
                -fsanitize=address,undefined
        )
    endif()
endfunction()
