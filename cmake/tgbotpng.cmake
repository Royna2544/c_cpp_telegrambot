include(cmake/FindWebP.cmake)
find_package(PNG REQUIRED)
find_package(JPEG REQUIRED)
find_package(OpenCV REQUIRED)

add_library_san(TgBotImgProc SHARED
    src/imagep/ImageProcOpenCV.cpp
    src/imagep/ImageTypeWEBP.cpp
    src/imagep/ImageTypeJPEG.cpp
    src/imagep/ImageTypePNG.cpp)
target_include_directories(TgBotImgProc PUBLIC 
    ${PNG_INCLUDE_DIR} ${WebP_INCLUDE_DIRS} ${JPEG_INCLUDE_DIRS} ${OpenCV_INCLUDE_DIRS})
target_link_libraries(TgBotImgProc 
    ${PNG_LIBRARIES} ${WebP_LIBRARIES} ${JPEG_LIBRARIES} ${OpenCV_LIBS})
