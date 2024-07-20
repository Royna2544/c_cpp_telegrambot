find_package(LibXml2 REQUIRED)

# TgBotStringRes library
add_my_library(
  NAME StringRes 
  SRCS StringResLoader.cpp
  LIBS ${LIBXML2_LIBRARIES}
  PUBLIC_INC .
  PRIVATE_INC ${LIBXML2_INCLUDE_DIR}
  STATIC
)

add_my_library(
  NAME StringResManager
  SRCS StringResManager.cpp
  LIBS TgBotStringRes TgBotUtils
  DEPENDS gen_stringres_header
)

add_my_executable(
  NAME StringResCompiler
  SRCS StringResCompiler.cpp
  LIBS TgBotStringRes
  NO_PREFIX
)

set(STRINGRES_XML ${CMAKE_SOURCE_DIR}/resources/strings/en-US.xml)
set(STRINGRES_GENHDR_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(STRINGRES_GENHDR ${STRINGRES_GENHDR_DIR}/resources.gen.h)
set(STRINGRES_COMPILER ${CMAKE_BINARY_DIR}/bin/StringResCompiler)

add_custom_command(
  OUTPUT ${STRINGRES_GENHDR}
  DEPENDS ${STRINGRES_COMPILER} ${STRINGRES_XML}
  COMMAND cmake -E make_directory ${STRINGRES_GENHDR_DIR}
  COMMAND ${STRINGRES_COMPILER} ${STRINGRES_XML} ${STRINGRES_GENHDR}
)
add_custom_target(gen_stringres_header
  COMMENT "Generating string resource header"
  DEPENDS ${STRINGRES_XML} ${STRINGRES_GENHDR})
set_source_files_properties(${STRINGRES_GENHDR} PROPERTIES GENERATED TRUE)
target_include_directories(TgBotStringResManager PUBLIC ${STRINGRES_GENHDR_DIR})
