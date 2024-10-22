add_my_library(
    NAME _popen_wdt
    SRCS
        popen_wdt_${TARGET_VARIANT}.c
        popen_wdt.c
    STATIC
)