set(EXPORTCONF_DIR "${CMAKE_BINARY_DIR}/exportConfig")

# Optional components group
add_custom_target(optional_all)

# Library install directory
include(GNUInstallDirs)
if(WIN32)
  set(LIBRARY_INSTALL_DIR ${CMAKE_INSTALL_BINDIR})
else()
  set(LIBRARY_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR})
endif()

# ################# Declare common macros ####################
function(tgbot_common_target TARGET_NAME)
  # ---------- generic properties & tooling ----------
  target_include_directories(
    ${TARGET_NAME}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${EXPORTCONF_DIR} ${CMAKE_CURRENT_BINARY_DIR})

  target_link_libraries(
    ${TARGET_NAME}
    PRIVATE fmt::fmt-header-only absl::log
  )

  # Set RPATH for Unix-like systems
  if (UNIX)
    # bin/main bin/cli lib/libTgBot.so
    set_target_properties(${TARGET_NAME} PROPERTIES INSTALL_RPATH_USE_LINK_PATH
                                                    TRUE)
    set_target_properties(${TARGET_NAME} PROPERTIES INSTALL_RPATH
                                                    "$ORIGIN/../lib;$ORIGIN/..")
  endif()

  # Output directory
  set_target_properties(
    ${TARGET_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                              ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
  set_target_properties(
    ${TARGET_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY
                              ${CMAKE_BINARY_DIR}/${LIBRARY_INSTALL_DIR})

  # LTO
  if (LTO_SUPPORTED)
    set_target_properties(${TARGET_NAME} PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
  endif()

  # Global variables
  target_link_libraries(${TARGET_NAME} PRIVATE GlobalCompilerSettings)
endfunction()

function(tgbot_library)
  cmake_parse_arguments(LIB "ALWAYS_STATIC" "NAME" "SRCS" ${ARGN})

  if (LIB_ALWAYS_STATIC)
    add_library(${LIB_NAME} STATIC ${LIB_SRCS})
  else()
    add_library(${LIB_NAME} ${LIB_SRCS})
    list(APPEND LIB_PUBLIC_INC ${EXPORTCONF_DIR})
    if (WIN32)
        set_target_properties(${LIB_NAME} PROPERTIES PREFIX "${CMAKE_SHARED_LIBRARY_PREFIX}${PROJECT_NAME}.")
    else()
        set_target_properties(${LIB_NAME} PROPERTIES PREFIX "${CMAKE_SHARED_LIBRARY_PREFIX}${PROJECT_NAME}")
    endif()
    set_target_properties(${LIB_NAME} PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
    set_target_properties(${LIB_NAME} PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES)
  endif()

  tgbot_common_target(${LIB_NAME})

  file(MAKE_DIRECTORY "${EXPORTCONF_DIR}")

  if (NOT LIB_ALWAYS_STATIC)
    # Generate export header for shared libraries
    include(GenerateExportHeader)
    generate_export_header(${LIB_NAME} BASE_NAME ${LIB_NAME} EXPORT_FILE_NAME
                            "${EXPORTCONF_DIR}/${LIB_NAME}Exports.h")
  endif()
  if (BUILD_SHARED_LIBS AND NOT LIB_ALWAYS_STATIC)
    install(TARGETS ${LIB_NAME} DESTINATION ${LIBRARY_INSTALL_DIR})
  endif()
endfunction()

function(tgbot_exe)
  cmake_parse_arguments(EXE "OPTIONAL;TEST" "NAME;RELATION" "SRCS" ${ARGN})

  # Tests get "test_" prefix
  if(EXE_TEST)
    set(TARGET_NAME test_${EXE_NAME})
  else()
    set(TARGET_NAME ${EXE_NAME})
  endif()

  if (EXE_OPTIONAL)
    add_executable(${TARGET_NAME} EXCLUDE_FROM_ALL ${EXE_SRCS} ${CMAKE_SOURCE_DIR}/src/logging/WrapMain.cpp)
    add_dependencies(optional_all ${TARGET_NAME})
  else()
    add_executable(${TARGET_NAME} ${EXE_SRCS} ${CMAKE_SOURCE_DIR}/src/logging/WrapMain.cpp)
  endif()
  target_link_libraries(${TARGET_NAME} PRIVATE absl::log_initialize absl::log_sink_registry)
  set_target_properties(${TARGET_NAME} PROPERTIES PREFIX "${PROJECT_NAME}_")

  tgbot_common_target(${TARGET_NAME})

  if (EXE_TEST)
    # ---------- extra test registration ----------
    add_test(NAME TestSuite_${TARGET_NAME} COMMAND ${TARGET_NAME})
    set_tests_properties(TestSuite_${TARGET_NAME} PROPERTIES
	ENVIRONMENT "GLIDER_ROOT=${CMAKE_BINARY_DIR}"
    )
  elseif(NOT EXE_OPTIONAL)
    if(NOT EXE_RELATION)
      message(
        SEND_ERROR
          "Please specify RELATION for ${TARGET_NAME}. Valid: Main | Socket | Database"
      )
    endif()
    install(TARGETS ${TARGET_NAME} COMPONENT App${EXE_RELATION})
  endif()
endfunction()
