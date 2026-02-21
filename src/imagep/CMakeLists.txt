find_package(PNG)
find_package(JPEG)
find_package(OpenCV COMPONENTS core imgproc highgui)
find_package(WebP)

set(IMGPROC_SOURCES)
set(IMGPROC_FLAGS)
set(IMGPROC_LIBS)

if(WebP_FOUND)
  list(APPEND IMGPROC_FLAGS -DIMAGEPROC_HAVE_LIBWEBP)
  list(APPEND IMGPROC_SOURCES ImageTypeWEBP.cpp)
  list(APPEND IMGPROC_LIBS WebP::webp)
  message(STATUS "libTgBotImgProc: libWebP Present")
endif()
if(JPEG_FOUND)
  list(APPEND IMGPROC_FLAGS -DIMAGEPROC_HAVE_LIBJPEG)
  list(APPEND IMGPROC_SOURCES ImageTypeJPEG.cpp)
  list(APPEND IMGPROC_LIBS JPEG::JPEG)
  message(STATUS "libTgBotImgProc: libJPEG Present")
endif()
if(PNG_FOUND)
  list(APPEND IMGPROC_FLAGS -DIMAGEPROC_HAVE_LIBPNG)
  list(APPEND IMGPROC_SOURCES ImageTypePNG.cpp)
  list(APPEND IMGPROC_LIBS PNG::PNG)
  message(STATUS "libTgBotImgProc: libPNG Present")
endif()
if(OpenCV_FOUND)
  list(APPEND IMGPROC_FLAGS -DIMAGEPROC_HAVE_OPENCV)
  list(APPEND IMGPROC_SOURCES ImageProcOpenCV.cpp)
  list(APPEND IMGPROC_LIBS ${OpenCV_LIBS})
  message(STATUS "libTgBotImgProc: OpenCV Present")
endif()

tgbot_library(
  NAME ImgProc
  SRCS
    ${IMGPROC_SOURCES}
    ImageDebug.cpp
    ImageProcAll.cpp
)

target_include_directories(ImgProc PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(ImgProc PRIVATE ${IMGPROC_LIBS})
target_compile_definitions(ImgProc PRIVATE ${IMGPROC_FLAGS})

# https://stackoverflow.com/a/46515541
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  target_link_options(ImgProc PUBLIC -Wl,-Bsymbolic)
endif()
