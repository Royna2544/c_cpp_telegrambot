find_package(LibXml2 REQUIRED)

# TgBotStringRes library
add_library_san(TgBotStringRes SHARED src/stringres/StringResLoader.cpp)
target_link_libraries(TgBotStringRes ${LIBXML2_LIBRARIES})
target_include_directories(TgBotStringRes PUBLIC src/stringres)
target_include_directories(TgBotStringRes PRIVATE ${LIBXML2_INCLUDE_DIR})
add_executable_san(StringResCompiler src/stringres/StringResCompiler.cpp)
target_link_libraries(StringResCompiler TgBotStringRes TgBotLogInit)

set(STRINGRES_XML ${CMAKE_SOURCE_DIR}/resources/strings/en-US.xml)
set(STRINGRES_GENHDR_DIR ${CMAKE_BINARY_DIR}/src/stringres/)
set(STRINGRES_GENHDR ${STRINGRES_GENHDR_DIR}/resources.gen.h)
set(STRINGRES_COMPILER ${CMAKE_BINARY_DIR}/bin/StringResCompiler)
add_custom_command(
  OUTPUT ${STRINGRES_GENHDR}
  DEPENDS ${STRINGRES_COMPILER} ${STRINGRES_XML}
  COMMAND ${STRINGRES_COMPILER} ${STRINGRES_XML} ${STRINGRES_GENHDR}
)
add_custom_target(gen_stringres_header
  COMMENT "Generating string resource header"
  DEPENDS ${STRINGRES_XML} ${STRINGRES_GENHDR})
set_source_files_properties(${STRINGRES_GENHDR} PROPERTIES GENERATED TRUE)