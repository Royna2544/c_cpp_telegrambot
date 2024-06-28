################ Define a CMake Test Style macro ################
macro(perform_test msg)
  message(STATUS "Performing test ${msg}")
  set(PERFORM_TEST_SUCCESS "Performing test ${msg}: Success")
  set(PERFORM_TEST_FAIL "Performing test ${msg}: Failed")
endmacro()
macro(perform_test_ret result)
  if (${result})
    message(STATUS ${PERFORM_TEST_SUCCESS})
  else()
    message(STATUS ${PERFORM_TEST_FAIL})
  endif()
endmacro()
########### Define a macro to prevent set(VAR ${VAR} NewVar) ###########
macro(extend_set var)
  set(${var} ${${var}} ${ARGN})
endmacro()
macro(extend_set_if var cond)
  if(${cond})
    extend_set(${var} ${ARGN})
  endif()
endmacro()
########### Define a target_link_libraries helper macro ###########
macro(target_link_lib_if_windows target)
  if (WIN32)
    target_link_libraries(${target} ${ARGN})
  endif()
endmacro()
#####################################################################