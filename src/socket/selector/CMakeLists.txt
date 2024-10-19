# Selectors: e.g. epoll, poll, select etc
if (UNIX)
  set(SELECTOR_SRC_LIST
    SelectorPosixPoll.cpp
    SelectorPosixSelect.cpp
    SelectorUnix.cpp
  )
  if (APPLE)
  list(APPEND SELECTOR_SRC_LIST
      SelectorLinuxEPollStub.cpp
    )
  else()
    list(APPEND SELECTOR_SRC_LIST
      SelectorLinuxEPoll.cpp
    )
  endif()
elseif(WIN32)
  list(APPEND SELECTOR_SRC_LIST 
    SelectorWindowsSelect.cpp
  )
endif()

add_my_library(
  NAME SocketSelectors
  SRCS ${SELECTOR_SRC_LIST}
  LIBS TgBotUtils
  LIBS_WIN32 Ws2_32
  PUBLIC_INC .
)