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
if(OpenCV_FOUND AND NOT SANITIZER_CONFIG STREQUAL "ASan")
  list(APPEND IMGPROC_FLAGS -DIMAGEPROC_HAVE_OPENCV)
  list(APPEND IMGPROC_SOURCES ImageProcOpenCV.cpp)
  list(APPEND IMGPROC_LIBS ${OpenCV_LIBS})
  message(STATUS "libTgBotImgProc: OpenCV Present")
else()
  message(
    STATUS
      "libTgBotImgProc: OpenCV_FOUND ${OpenCV_FOUND}, SanitizerConfig ${SANITIZER_CONFIG}"
  )
endif()

tgbot_library(
  NAME
  ImgProc
  SRCS
  ${IMGPROC_SOURCES}
  ImageProcAll.cpp
  PRIVATE_INC
  ${OpenCV_INCLUDE_DIRS}
  LIBS
  ${IMGPROC_LIBS}
  absl::status
  DEFS
  ${IMGPROC_FLAGS})
