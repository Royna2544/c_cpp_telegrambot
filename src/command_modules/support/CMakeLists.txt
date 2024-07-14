add_library(TgBot_popen_wdt STATIC
    popen_wdt/popen_wdt_${TARGET_VARIANT}.c
    popen_wdt/popen_wdt.c
)
target_include_directories(TgBot_popen_wdt PUBLIC popen_wdt)

add_library(TgBot_portable_semaphore STATIC 
    portable_sem/portable_sem.c
)
target_include_directories(TgBot_portable_semaphore PUBLIC portable_sem)
