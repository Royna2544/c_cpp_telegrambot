add_my_library(
    NAME minimalhash
    SRCS sha256.cpp hmac.cpp
    LIBS OpenSSL::Crypto
    STATIC
)