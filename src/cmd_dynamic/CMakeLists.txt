set(DYNAMIC_CMDS_LIST DynTest)

FILE(REMOVE ${CMAKE_SOURCE_DIR}/modules.load)
foreach(lib ${DYNAMIC_CMDS_LIST})
	add_library(cmd_${lib} SHARED cmd_${lib}.cpp)
	target_link_libraries(cmd_${lib} TgBot)
	FILE(APPEND ${CMAKE_SOURCE_DIR}/modules.load libcmd_${lib}.so\n)
endforeach()
execute_process(COMMAND ln -sf src/cmd_dynamic modules)
