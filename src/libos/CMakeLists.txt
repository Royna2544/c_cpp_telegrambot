tgbot_library(
   NAME Signal
   SRCS 
     libsighandler_impl.cpp
     libsighandler_${TARGET_VARIANT}.cpp
   STATIC
)
