add_my_library(
    NAME sighandler
    SRCS 
        libsighandler_impl.cpp
        libsighandler_${TARGET_VARIANT}.cpp
    STATIC
)