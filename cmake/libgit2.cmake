# libgit2
add_subdirectory(src/third-party/libgit2 EXCLUDE_FROM_ALL)
set(LIBGIT2_LIBS libgit2 util pcre http-parser xdiff ${ZLIB_LIBRARIES} ${OPENSSL_LIBRARIES})
if (WIN32)
  extend_set(LIBGIT2_LIBS winhttp CRYPT32 Rpcrt4 Secur32 Ws2_32)
elseif(APPLE)
  extend_set(LIBGIT2_LIBS ntlmclient iconv ${MACOS_SSL_LIBS})
else()
  extend_set(LIBGIT2_LIBS ntlmclient)
endif()