# Initialize DYNAMIC_CMDS_LIST with base commands
set(DYNAMIC_CMDS_LIST DynTest DTState)

# Extend DYNAMIC_CMDS_LIST based on conditions
if (ENABLE_RUST_SUPPORT)
    list(APPEND DYNAMIC_CMDS_LIST StringCalc)
endif()

if (UNIX AND NOT APPLE)
    list(APPEND DYNAMIC_CMDS_LIST RomBuild)
endif()

# Remove existing modules.load file
file(REMOVE ${CMAKE_SOURCE_DIR}/modules.load)

# Iterate over DYNAMIC_CMDS_LIST to add libraries
foreach(lib ${DYNAMIC_CMDS_LIST})
	if (lib STREQUAL "RomBuild" AND UNIX AND NOT APPLE)
    	add_library(cmd_${lib} SHARED cmd_${lib}.cpp ../../android_builder/tasks/ROMBuildTask.cpp)
	else()
    	add_library(cmd_${lib} SHARED cmd_${lib}.cpp)
	endif()
    target_link_libraries(cmd_${lib} ${PROJECT_NAME})
    file(APPEND ${CMAKE_SOURCE_DIR}/modules.load "libcmd_${lib}\n")
    
    # Additional linking based on conditions
    if (lib STREQUAL "StringCalc" AND ENABLE_RUST_SUPPORT)
        target_link_libraries(cmd_${lib} stringcalc)
    endif()

    if (lib STREQUAL "RomBuild" AND UNIX AND NOT APPLE)
        target_link_libraries(cmd_${lib} AndroidBuilder-cpp TgBotUtils)
    endif()
endforeach()
