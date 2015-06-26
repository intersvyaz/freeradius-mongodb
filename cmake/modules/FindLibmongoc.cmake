# - Find libmongoc
# Find the native libmongoc includes and library.
# Once done this will define
#
#  LIBMONGOC_INCLUDE_DIRS - where to find mongoc.h, etc.
#  LIBMONGOC_LIBRARIES    - List of libraries when using libmongoc.
#  LIBMONGOC_FOUND        - True if libmongoc found.
#
#  LIBMONGOC_VERSION_STRING - The version of libmongoc found (x.y.z)
#  LIBMONGOC_VERSION_MAJOR  - The major version
#  LIBMONGOC_VERSION_MINOR  - The minor version
#  LIBMONGOC_VERSION_MICRO  - The micro version

FIND_PATH(LIBMONGOC_INCLUDE_DIR NAMES mongoc.h PATH_SUFFIXES libmongoc-1.0)
FIND_LIBRARY(LIBMONGOC_LIBRARY  NAMES mongoc-1.0)

MARK_AS_ADVANCED(LIBMONGOC_LIBRARY LIBMONGOC_INCLUDE_DIR)

IF(LIBMONGOC_INCLUDE_DIR AND EXISTS "${LIBMONGOC_INCLUDE_DIR}/mongoc-version.h")
    # Read and parse version header file for version number
    file(READ "${LIBMONGOC_INCLUDE_DIR}/mongoc-version.h" _libmongoc_HEADER_CONTENTS)
    IF(_libmongoc_HEADER_CONTENTS MATCHES ".*MONGOC_MAJOR_VERSION.*")
        string(REGEX REPLACE ".*#define +MONGOC_MAJOR_VERSION +\\(([0-9]+)\\).*" "\\1" LIBMONGOC_VERSION_MAJOR "${_libmongoc_HEADER_CONTENTS}")
        string(REGEX REPLACE ".*#define +MONGOC_MINOR_VERSION +\\(([0-9]+)\\).*" "\\1" LIBMONGOC_VERSION_MINOR "${_libmongoc_HEADER_CONTENTS}")
        string(REGEX REPLACE ".*#define +MONGOC_MICRO_VERSION +\\(([0-9]+)\\).*" "\\1" LIBMONGOC_VERSION_MICRO "${_libmongoc_HEADER_CONTENTS}")
    ELSE()
       SET(LIBMONGOC_VERSION_MAJOR 0)
       SET(LIBMONGOC_VERSION_MINOR 0)
       SET(LIBMONGOC_VERSION_MICRO 0)
    ENDIF()

    SET(LIBMONGOC_VERSION_STRING "${LIBMONGOC_VERSION_MAJOR}.${LIBMONGOC_VERSION_MINOR}.${LIBMONGOC_VERSION_MICRO}")
ENDIF()

# handle the QUIETLY and REQUIRED arguments and set LIBMONGOC_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Libmongoc
    REQUIRED_VARS LIBMONGOC_LIBRARY LIBMONGOC_INCLUDE_DIR
    VERSION_VAR LIBMONGOC_VERSION_STRING
)

IF(LIBMONGOC_FOUND)
    SET(LIBMONGOC_INCLUDE_DIRS ${LIBMONGOC_INCLUDE_DIR})
    SET(LIBMONGOC_LIBRARIES ${LIBMONGOC_LIBRARY})
ENDIF()
