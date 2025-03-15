find_package(LibArchive REQUIRED)

tgbot_library(
   NAME Archives
   SRCS Tar.cpp Zip.cpp
   LIBS LibArchive::LibArchive
   STATIC
)
