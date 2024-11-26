# Locate libzip
# This module defines
# LIBZIP_LIBRARY
# LIBZIP_FOUND, if false, do not try to link to libzip
# LIBZIP_INCLUDE_DIR, where to find the headers
#

FIND_PATH(LIBZIP_INCLUDE_DIR zip.h
    $ENV{LIBZIP_DIR}/include
    $ENV{LIBZIP_DIR}
    /usr/local/include
    /usr/include
)

FIND_LIBRARY(LIBZIP_LIBRARY
    NAMES libzip zip
    PATHS
    $ENV{LIBZIP_DIR}/lib
    $ENV{LIBZIP_DIR}
    /usr/local/lib
    /usr/lib

)

SET(LIBZIP_FOUND "NO")
IF(LIBZIP_LIBRARY AND LIBZIP_INCLUDE_DIR)
    SET(LIBZIP_FOUND "YES")
    MESSAGE(STATUS "Found libzip: ${LIBZIP_LIBRARY}")
    ADD_LIBRARY(Zip::Zip SHARED IMPORTED)
    SET_TARGET_PROPERTIES(Zip::Zip PROPERTIES
        IMPORTED_LOCATION ${LIBZIP_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES ${LIBZIP_INCLUDE_DIR}
    )
ENDIF(LIBZIP_LIBRARY AND LIBZIP_INCLUDE_DIR)