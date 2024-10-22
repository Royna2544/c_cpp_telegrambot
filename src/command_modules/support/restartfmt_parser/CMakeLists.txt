add_my_library(
    NAME _restartfmt_parser
    SRCS
        restartfmt_parser.cpp
    LIBS
        absl::strings absl::status TgBotPPImpl_shared_deps
    STATIC
)