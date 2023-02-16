cmake_minimum_required(VERSION 3.10.2)
project(tg_cpphost)

add_subdirectory(lib)
set(SRC_LIST
    src/main.cpp)

add_executable(${PROJECT_NAME} ${SRC_LIST})

target_link_libraries(${PROJECT_NAME} TgBot)
