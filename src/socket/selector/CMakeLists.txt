# Selectors: e.g. epoll, poll, select etc
if (UNIX)
  set(SELECTOR_SRC_LIST
    SelectorPosixPoll.cpp
    SelectorPosixSelect.cpp
    SelectorUnix.cpp
  )
  if(LINUX OR ANDROID)
    list(APPEND SELECTOR_SRC_LIST
      SelectorLinuxEPoll.cpp
    )
  endif()
else()
  return()
endif()

tgbot_library(
  NAME SocketSelectors
  SRCS ${SELECTOR_SRC_LIST}
)
