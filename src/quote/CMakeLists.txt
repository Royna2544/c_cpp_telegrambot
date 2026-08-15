find_package(PkgConfig REQUIRED)
pkg_check_modules(PANGOCAIRO REQUIRED IMPORTED_TARGET pangocairo)
pkg_check_modules(PANGOFC REQUIRED IMPORTED_TARGET pangofc)
pkg_check_modules(FONTCONFIG REQUIRED IMPORTED_TARGET fontconfig)
find_package(PNG REQUIRED)
find_package(libjpeg-turbo CONFIG REQUIRED)
find_package(WebP REQUIRED)

# OpenCV is deliberately optional here.  The regular desktop build uses it for
# Telegram image decoding (including EXIF orientation), while the ARMHF and
# lightweight CodeQL configurations only enable the `image` vcpkg feature.  A
# bounded PNG/JPEG/WebP decoder below keeps the native quote renderer available
# in those configurations instead of making `image-opencv` an accidental hard
# dependency.
find_package(OpenCV QUIET COMPONENTS core imgproc imgcodecs)

tgbot_library(
  NAME QuoteRenderer
  SRCS QuoteRenderer.cpp
  ALWAYS_SHARED
)

target_include_directories(QuoteRenderer
  PUBLIC
    ${CMAKE_SOURCE_DIR}/src/include
    ${EXPORTCONF_DIR}
  PRIVATE
    ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(QuoteRenderer
  PRIVATE
    PkgConfig::PANGOCAIRO
    PkgConfig::PANGOFC
    PkgConfig::FONTCONFIG
    PNG::PNG
    $<IF:$<TARGET_EXISTS:libjpeg-turbo::turbojpeg>,libjpeg-turbo::turbojpeg,libjpeg-turbo::turbojpeg-static>
    WebP::webp
)

if(CMAKE_DL_LIBS)
  target_link_libraries(QuoteRenderer PRIVATE ${CMAKE_DL_LIBS})
endif()

if(OpenCV_FOUND)
  target_compile_definitions(QuoteRenderer PRIVATE QUOTE_HAVE_OPENCV)
  target_link_libraries(QuoteRenderer PRIVATE ${OpenCV_LIBS})
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  target_link_options(QuoteRenderer PRIVATE -Wl,-Bsymbolic)
endif()
