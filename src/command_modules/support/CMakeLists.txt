add_my_library(
    NAME _popen_wdt
    SRCS
        popen_wdt/popen_wdt_${TARGET_VARIANT}.c
        popen_wdt/popen_wdt.c
    PUBLIC_INC
        popen_wdt
    STATIC
)

add_my_library(
    NAME _portable_semaphore
    SRCS
        portable_sem/portable_sem.c
    PUBLIC_INC
        portable_sem
    STATIC
)

add_my_library(
    NAME _restartfmt_parser
    SRCS
        restartfmt_parser/restartfmt_parser.cpp
    PUBLIC_INC
        restartfmt_parser
    LIBS
        absl::strings absl::status TgBotPPImpl_shared_deps
    STATIC
)
