set(SANITIZE_ADDRESS OFF)  # Enable ASanitizer
set(SANITIZE_THREAD OFF)  # Disable TSanitizer
set(SANITIZE_UNDEFINED OFF)  # Enable UBSan

# MSYS2 is special - They don't have any sanitizers
if (WIN32)
    set(SANITIZE_ADDRESS OFF)
    set(SANITIZE_UNDEFINED OFF)
endif()

macro(add_sanitizers target)
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
endmacro()

# Convenience function
macro(add_executable_san target)
    add_executable(${target} ${ARGN})

    add_sanitizers(${target})
endmacro()

# Convenience function
macro(add_library_san target)
    add_library(${target} ${ARGN})

    add_sanitizers(${target})
endmacro()
