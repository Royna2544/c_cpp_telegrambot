function(MY_PROTOBUF_GENERATE_CPP PATH SRCS HDRS)
  if(NOT ARGN)
    message(
      SEND_ERROR "Error: PROTOBUF_GENERATE_CPP() called without any proto files"
    )
    return()
  endif()

  if(PROTOBUF_GENERATE_CPP_APPEND_PATH)
    # Create an include path for each file specified
    foreach(FIL ${ARGN})
      get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
      get_filename_component(ABS_PATH ${ABS_FIL} PATH)
      list(FIND _protobuf_include_path ${ABS_PATH} _contains_already)
      if(${_contains_already} EQUAL -1)
        list(APPEND _protobuf_include_path -I ${ABS_PATH})
      endif()
    endforeach()
  else()
    set(_protobuf_include_path -I ${CMAKE_CURRENT_SOURCE_DIR})
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

    set(PROTO_GENSRC_ROOT "${CMAKE_CURRENT_BINARY_DIR}/${PATH}")
    set(PROTO_GENSRC_OUT "${CMAKE_CURRENT_BINARY_DIR}/${PATH}/proto")
    set(PROTO_GEN_SRC "${FIL_WE}.pb.cc")
    set(PROTO_GEN_HDR "${FIL_WE}.pb.h")
    list(APPEND ${SRCS} "${PROTO_GENSRC_OUT}/${PROTO_GEN_SRC}")
    list(APPEND ${HDRS} "${PROTO_GENSRC_OUT}/${PROTO_GEN_HDR}")

    set(PROTO_GEN_SRCS "${PROTO_GENSRC_ROOT}/${PROTO_GEN_SRC}"
                       "${PROTO_GENSRC_ROOT}/${PROTO_GEN_HDR}")
    execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory
                            ${PROTO_GENSRC_ROOT})

    add_custom_target(protobuf_${FIL_WE}_ready
      DEPENDS ${PROTO_GEN_SRCS}
      COMMENT "Generated ${FIL_WE} Proto Srcs..."
    )
    add_custom_command(
      OUTPUT ${PROTO_GEN_SRCS}
      COMMAND
        protoc ARGS --cpp_out
        ${CMAKE_CURRENT_BINARY_DIR}/${PATH} ${_protobuf_include_path} ${ABS_FIL}
      DEPENDS ${ABS_FIL} protoc
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
