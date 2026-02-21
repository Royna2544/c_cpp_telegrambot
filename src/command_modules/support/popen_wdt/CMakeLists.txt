tgbot_library(
  NAME popen_wdt
  SRCS
  popen_wdt.c
  popen_wdt_${TARGET_VARIANT}.c
  $<$<BOOL:${UNIX}>:portable_sem.c>
  ALWAYS_STATIC
)
