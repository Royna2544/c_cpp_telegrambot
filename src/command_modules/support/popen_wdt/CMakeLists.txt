tgbot_library(
    NAME popen_wdt
    SRCS popen_wdt.c popen_wdt_${TARGET_VARIANT}.c
    SRCS_UNIX portable_sem.c
    STATIC
)