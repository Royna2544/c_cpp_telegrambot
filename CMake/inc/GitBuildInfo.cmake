
string(TIMESTAMP TODAY "%Y-%m-%d %H:%M:%S UTC" UTC)

find_package(Git REQUIRED) # It will obviously have
function(git_execute_proc)
  cmake_parse_arguments(
    GIT_PROC
    "" # Option
    "NAME;VAR" # Single
    "COMMAND" # Multiple
    ${ARGN})
  if(NOT
     (GIT_PROC_NAME
      AND GIT_PROC_VAR
      AND GIT_PROC_COMMAND))
    message(SEND_ERROR "Missing arguments")
  endif()

  # Run the git command to get the commit ID or other info
  execute_process(
    COMMAND ${GIT_EXECUTABLE} ${GIT_PROC_COMMAND}
    OUTPUT_VARIABLE GIT_PROC_OUT
    ERROR_VARIABLE GIT_PROC_ERR
    RESULT_VARIABLE GIT_PROC_RESULT
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  # Check if the git command was successful
  if(GIT_PROC_RESULT EQUAL 0)
    set(${GIT_PROC_VAR}
        ${GIT_PROC_OUT}
        PARENT_SCOPE)
    message(STATUS "Git ${GIT_PROC_NAME}: ${GIT_PROC_OUT}")
  else()
    set(${GIT_PROC_VAR}
        "-"
        PARENT_SCOPE)
    message(WARNING "Error retrieving Git ${GIT_PROC_NAME}: ${GIT_PROC_ERR}")
  endif()
endfunction()

git_execute_proc(
  COMMAND
  rev-parse
  HEAD
  NAME
  "commit-id"
  VAR
  GIT_COMMIT_ID)

git_execute_proc(
  COMMAND
  log
  -1
  --pretty=%B
  NAME
  "commit-message"
  VAR
  GIT_COMMIT_MESSAGE)

git_execute_proc(
  COMMAND
  remote
  get-url
  origin
  NAME
  "origin-url"
  VAR
  GIT_ORIGIN_URL)

configure_file(resources/about.html.in ${CMAKE_SOURCE_DIR}/resources/about.html)
configure_file(src/include/GitBuildInfo.hpp.inc
               ${CMAKE_SOURCE_DIR}/src/include/GitBuildInfo.hpp)