include(cmake/FindWebP.cmake)
find_package(PNG)
find_package(JPEG)
find_package(OpenCV COMPONENTS core imgproc highgui)

set(TGBOTPNG_SOURCES)
set(TGBOTPNG_FLAGS)
set(TGBOTPNG_LIBS)

if (${WebP_FOUND})
    extend_set(TGBOTPNG_FLAGS -DHAVE_LIBWEBP)
    extend_set(TGBOTPNG_SOURCES src/imagep/ImageTypeWEBP.cpp)
    message(STATUS "libWebP Present")
endif()
if (${JPEG_FOUND})
    extend_set(TGBOTPNG_FLAGS -DHAVE_LIBJPEG)
    extend_set(TGBOTPNG_SOURCES src/imagep/ImageTypeJPEG.cpp)
    message(STATUS "libJPEG Present")
endif()
if (${PNG_FOUND})
    extend_set(TGBOTPNG_FLAGS -DHAVE_LIBPNG)
    extend_set(TGBOTPNG_SOURCES src/imagep/ImageTypePNG.cpp)
    message(STATUS "libPNG Present")
endif()
if (${OpenCV_FOUND})
    extend_set(TGBOTPNG_FLAGS -DHAVE_OPENCV)
    extend_set(TGBOTPNG_SOURCES src/imagep/ImageProcOpenCV.cpp)
    message(STATUS "OpenCV Present")
endif()

add_library_san(TgBotImgProc SHARED
    ${TGBOTPNG_SOURCES}
    src/imagep/ImageProcAll.cpp)
    
target_compile_definitions(TgBotImgProc PRIVATE ${TGBOTPNG_FLAGS})
target_include_directories(TgBotImgProc PRIVATE 
    ${PNG_INCLUDE_DIR} ${WebP_INCLUDE_DIRS} ${JPEG_INCLUDE_DIRS} ${OpenCV_INCLUDE_DIRS})
target_link_libraries(TgBotImgProc 
    ${PNG_LIBRARIES} ${WebP_LIBRARIES} ${JPEG_LIBRARIES} ${OpenCV_LIBS})
