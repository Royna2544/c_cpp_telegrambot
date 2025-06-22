# --- FlatcFun.cmake ---------------------------------------------------------
# add_flatbuffer_schema(
#     TARGET  <logical-target-name>
#     SCHEMA  <.fbs file, relative or absolute>
#     LANGS   <cc|cpp|csharp|java|python|ts|go|rust …>  # any flatc code-gen flags
#     FLAGS   <extra flatc flags>                       # optional
#     OUT_DIR <where to put generated files>            # optional
# )
#
# Creates:
#   • a custom command that runs flatc
#   • a custom target <TARGET> depending on generated outputs
# You can then use that target as a dependency or bundle
# several of them under another “all” target.
function(add_flatbuffer_schema)
    cmake_parse_arguments(FLATC
        ""                                  # no single-value args
        "TARGET;SCHEMA;OUT_DIR"             # one-value args
        "LANGS;FLAGS"                       # multi-value args
        ${ARGN})

    if(NOT FLATC_TARGET OR NOT FLATC_SCHEMA)
        message(FATAL_ERROR "add_flatbuffer_schema: TARGET and SCHEMA are required")
    endif()

    # Resolve paths and names --------------------------------------------------
    get_filename_component(_schema_abs "${FLATC_SCHEMA}" ABSOLUTE)
    get_filename_component(_schema_name "${_schema_abs}" NAME_WE)

    # Where to put generated files
    set(_out_dir "${FLATC_OUT_DIR}")
    if(NOT _out_dir)
        set(_out_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
    endif()
    file(MAKE_DIRECTORY "${_out_dir}")

    # flatc output files we care about
    set(_hdr "${_out_dir}/${_schema_name}.generated.h")
    set(_bfbs "${_out_dir}/${_schema_name}.bfbs")

    # Build flatc command line -------------------------------------------------
    # Pull flatc from the imported target provided by FindFlatBuffers
    set(_flatc "$<TARGET_FILE:flatbuffers::flatc>")

    # Assemble language flags
    foreach(lang IN LISTS FLATC_LANGS)
        list(APPEND _langs "--${lang}")
    endforeach()

    add_custom_command(
        OUTPUT  "${_hdr}" "${_bfbs}"
        COMMAND "${_flatc}"
                ${_langs}
                --binary --schema
                -o "${_out_dir}"
                ${FLATC_FLAGS}
                "${_schema_abs}"
        DEPENDS "${_schema_abs}"
        COMMENT "FlatBuffers: Generating ${_schema_name} (${FLATC_LANGS})"
        VERBATIM
    )

    add_custom_target(${FLATC_TARGET} DEPENDS "${_hdr}" "${_bfbs}")
endfunction()
# ---------------------------------------------------------------------------
