include(FindWebP.cmake)
find_package(PNG)
find_package(JPEG)
find_package(OpenCV COMPONENTS core imgproc highgui)

set(TGBOTPNG_SOURCES)
set(TGBOTPNG_FLAGS)
set(TGBOTPNG_LIBS)

if (WebP_FOUND)
    list(APPEND TGBOTPNG_FLAGS -DIMAGEPROC_HAVE_LIBWEBP)
    list(APPEND TGBOTPNG_SOURCES ImageTypeWEBP.cpp)
    list(APPEND TGBOTPNG_LIBS ${WebP_LIBRARIES})
    message(STATUS "libTgBotImgProc: libWebP Present")
endif()
if (JPEG_FOUND)
    list(APPEND TGBOTPNG_FLAGS -DIMAGEPROC_HAVE_LIBJPEG)
    list(APPEND TGBOTPNG_SOURCES ImageTypeJPEG.cpp)
    list(APPEND TGBOTPNG_LIBS ${JPEG_LIBRARIES})
    message(STATUS "libTgBotImgProc: libJPEG Present")
endif()
if (PNG_FOUND)
    list(APPEND TGBOTPNG_FLAGS -DIMAGEPROC_HAVE_LIBPNG)
    list(APPEND TGBOTPNG_SOURCES ImageTypePNG.cpp)
    list(APPEND TGBOTPNG_LIBS ${PNG_LIBRARIES})
    message(STATUS "libTgBotImgProc: libPNG Present")
endif()
if (OpenCV_FOUND)
    list(APPEND TGBOTPNG_FLAGS -DIMAGEPROC_HAVE_OPENCV)
    list(APPEND TGBOTPNG_SOURCES ImageProcOpenCV.cpp)
    list(APPEND TGBOTPNG_LIBS ${OpenCV_LIBS})
    message(STATUS "libTgBotImgProc: OpenCV Present")
endif()

add_my_library(
    NAME ImgProc
    SRCS 
        ${TGBOTPNG_SOURCES}
        ImageProcAll.cpp
    PRIVATE_INC
        ${PNG_INCLUDE_DIR}
        ${WebP_INCLUDE_DIRS}
        ${JPEG_INCLUDE_DIRS}
        ${OpenCV_INCLUDE_DIRS}
    LIBS ${TGBOTPNG_LIBS} absl::status
    DEFS ${TGBOTPNG_FLAGS}
)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "i[3-6]86|x86_64|AMD64|amd64" AND NOT MSVC)
  target_compile_options(TgBotImgProc PRIVATE -msse2)
endif()