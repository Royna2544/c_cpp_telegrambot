find_package(Python COMPONENTS Development)

add_library_san(AndroidBuilder-cpp
    src/android_builder/PythonClass.cpp
    src/android_builder/ArgumentBuilder.cpp
    src/android_builder/RepoUtils.cpp
    src/android_builder/ForkAndRun.cpp
    src/android_builder/ConfigParsers.cpp
    src/android_builder/tasks/RepoSyncTask.cpp)
target_link_libraries(AndroidBuilder-cpp ${Python_LIBRARIES} TgBotLogInit TgBotSocket ${LIBGIT2_LIBS})
target_include_directories(AndroidBuilder-cpp PUBLIC ${Python_INCLUDE_DIRS})
target_include_directories(AndroidBuilder-cpp PUBLIC src/android_builder/)
