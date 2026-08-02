function(settings_enable_sanitizers target)
    if(MSVC)
        return()
    endif()

    set(flags)
    if(ASTREA_ENABLE_ASAN)
        list(APPEND flags address)
    endif()
    if(ASTREA_ENABLE_UBSAN)
        list(APPEND flags undefined)
    endif()
    if(NOT flags)
        return()
    endif()

    list(JOIN flags "," sanitizer_flags)
    target_compile_options(${target} PRIVATE
        -fsanitize=${sanitizer_flags}
        -fno-omit-frame-pointer
    )
    target_link_options(${target} PRIVATE
        -fsanitize=${sanitizer_flags}
    )
endfunction()
