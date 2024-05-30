function(MY_PROTOBUF_GENERATE_CPP SRCS HDRS)
  if(NOT ARGN)
    message(
      SEND_ERROR "Error: PROTOBUF_GENERATE_CPP() called without any proto files"
    )
    return()
  endif()

  if(DEFINED PROTOBUF_IMPORT_DIRS)
    foreach(DIR ${PROTOBUF_IMPORT_DIRS})
      get_filename_component(ABS_PATH ${DIR} ABSOLUTE)
      list(FIND _protobuf_include_path ${ABS_PATH} _contains_already)
      if(${_contains_already} EQUAL -1)
        list(APPEND _protobuf_include_path -I ${ABS_PATH})
      endif()
    endforeach()
  endif()

  set(${SRCS})
  set(${HDRS})
  foreach(FIL ${ARGN})
    get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
    get_filename_component(FIL_WE ${FIL} NAME_WE)
    get_filename_component(PATH_DIR ${FIL} DIRECTORY)
    get_filename_component(PATH_FILE ${FIL} NAME)

    set(PROTO_GENSRC_ROOT "${CMAKE_CURRENT_BINARY_DIR}/${PATH_DIR}")
    set(PROTO_GEN_SRC "${FIL_WE}.pb.cc")
    set(PROTO_GEN_HDR "${FIL_WE}.pb.h")
    list(APPEND ${SRCS} "${PROTO_GENSRC_ROOT}/${PROTO_GEN_SRC}")
    list(APPEND ${HDRS} "${PROTO_GENSRC_ROOT}/${PROTO_GEN_HDR}")

    set(PROTO_GEN_SRCS "${PROTO_GENSRC_ROOT}/${PROTO_GEN_SRC}"
                       "${PROTO_GENSRC_ROOT}/${PROTO_GEN_HDR}")
    set(PROTOC_EXE ${CMAKE_BINARY_DIR}/bin/protoc)
    execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory
                            ${PROTO_GENSRC_ROOT})
    add_custom_target(protobuf_${FIL_WE}_ready
      DEPENDS ${PROTO_GEN_SRCS}
      COMMENT "Generated ${FIL_WE} Proto Srcs..."
    )
    add_custom_command(
      OUTPUT ${PROTO_GEN_SRCS}
      COMMAND
        ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR}/${PATH_DIR} ${PROTOC_EXE} --cpp_out
        ${CMAKE_CURRENT_BINARY_DIR}/${PATH_DIR} ${_protobuf_include_path} ${PATH_FILE}
      DEPENDS ${ABS_FIL} ${PROTOC_EXE}
      COMMENT "Running C++ protocol buffer compiler on ${FIL}"
      VERBATIM)
  endforeach()

  set_source_files_properties(${${SRCS}} ${${HDRS}} PROPERTIES GENERATED TRUE)
  set(${SRCS}
      ${${SRCS}}
      PARENT_SCOPE)
  set(${HDRS}
      ${${HDRS}}
      PARENT_SCOPE)
endfunction()
