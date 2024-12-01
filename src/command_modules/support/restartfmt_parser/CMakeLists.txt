add_my_library(
    NAME restartfmt_parser
    SRCS
        restartfmt_parser.cpp
    LIBS
        absl::strings absl::status
    STATIC
    PRIVATE_INC $<TARGET_PROPERTY:TgBot,INTERFACE_INCLUDE_DIRECTORIES>
)