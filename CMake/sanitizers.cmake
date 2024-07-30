set(SANITIZE_ADDRESS ON)  # Enable ASanitizer
set(SANITIZE_THREAD OFF)  # Disable TSanitizer
set(SANITIZE_UNDEFINED ON)  # Enable UBSan

# MSYS2 is special - They don't have any sanitizers
if (WIN32)
    set(SANITIZE_ADDRESS OFF)
    set(SANITIZE_UNDEFINED OFF)
endif()

if (DISABLE_SANITIZERS)
    message(STATUS "Sanitizers disabled by option")
endif()

# Define a macro to add sanitizers to a target.
function(add_sanitizers target)
    if (DISABLE_SANITIZERS)
        return()
    endif()
    if (SANITIZE_ADDRESS)
        target_compile_options(${target} PRIVATE -fsanitize=address)
        target_link_options(${target} PRIVATE -fsanitize=address)
    endif()

    if (SANITIZE_THREAD)
        target_compile_options(${target} PRIVATE -fsanitize=thread)
        target_link_options(${target}  PRIVATE -fsanitize=thread)
    endif()

    if (SANITIZE_UNDEFINED)
        target_compile_options(${target} PRIVATE -fsanitize=undefined)
        target_link_options(${target} PRIVATE -fsanitize=undefined)
    endif()
endfunction()

