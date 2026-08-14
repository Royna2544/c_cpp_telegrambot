find_package(PkgConfig REQUIRED)
pkg_check_modules(PANGOCAIRO REQUIRED IMPORTED_TARGET pangocairo)
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
    PNG::PNG
    $<IF:$<TARGET_EXISTS:libjpeg-turbo::turbojpeg>,libjpeg-turbo::turbojpeg,libjpeg-turbo::turbojpeg-static>
    WebP::webp
)

# Keep a source-tree fallback for direct library/test execution. Installed and
# packaged builds resolve the same files relative to the executable first.
file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}/resources/quote/fonts" QUOTE_SOURCE_FONT_DIR)
target_compile_definitions(
  QuoteRenderer PRIVATE QUOTE_SOURCE_FONT_DIR="${QUOTE_SOURCE_FONT_DIR}"
)

if(OpenCV_FOUND)
  target_compile_definitions(QuoteRenderer PRIVATE QUOTE_HAVE_OPENCV)
  target_link_libraries(QuoteRenderer PRIVATE ${OpenCV_LIBS})
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  target_link_options(QuoteRenderer PRIVATE -Wl,-Bsymbolic)
endif()
