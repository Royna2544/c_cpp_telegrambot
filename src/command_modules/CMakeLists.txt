set(CMDS_LIST addblacklist rmblacklist addwhitelist 
			  rmwhitelist bash unsafebash alive flash
			  possibility decide delay decho randsticker
			  fileid starttimer stoptimer)

set(COMMANDS_CWD src/command_modules)
set(COMMANDS_GEN_HEADER ${CMAKE_BINARY_DIR}/cmds.gen.h)
FILE(REMOVE ${COMMANDS_GEN_HEADER})
FILE(APPEND ${COMMANDS_GEN_HEADER} "#include <vector>\n")
FILE(APPEND ${COMMANDS_GEN_HEADER} "#include <command_modules/CommandModule.h>\n\n")
foreach(lib ${CMDS_LIST})
	set(SRC_LIST ${SRC_LIST} ${COMMANDS_CWD}/${lib}.cpp)
	FILE(APPEND ${COMMANDS_GEN_HEADER} "extern const struct CommandModule cmd_${lib};\n")
endforeach()
FILE(APPEND ${COMMANDS_GEN_HEADER} "inline const std::vector gCmdModules = {\n")
foreach(lib ${CMDS_LIST})
	FILE(APPEND ${COMMANDS_GEN_HEADER} "cmd_${lib},\n")
endforeach()
FILE(APPEND ${COMMANDS_GEN_HEADER} "};\n")