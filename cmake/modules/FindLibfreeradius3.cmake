# - Find libfreeradius
# Find the native libfreeradius includes and library.
# Once done this will define
#
#  LIBFREERADIUS_INCLUDE_DIRS - where to find libfreeradius.h, etc.
#  LIBFREERADIUS_LIBRARIES    - List of libraries when using libfreeradius.
#  LIBFREERADIUS_FOUND        - True if libfreeradius found.

FIND_PATH(LIBFREERADIUS_INCLUDE_DIR NAMES freeradius/libradius.h)

FIND_LIBRARY(LIBFREERADIUS_LIBRARY
    NAMES freeradius-radius freeradius-eap
    PATH_SUFFIXES freeradius
)

MARK_AS_ADVANCED(LIBFREERADIUS_LIBRARY LIBFREERADIUS_INCLUDE_DIR)

# handle the QUIETLY and REQUIRED arguments and set LIBFREERADIUS_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Libfreeradius
    REQUIRED_VARS LIBFREERADIUS_LIBRARY LIBFREERADIUS_INCLUDE_DIR
)

IF(LIBFREERADIUS_FOUND)
    SET(LIBFREERADIUS_INCLUDE_DIRS ${LIBFREERADIUS_INCLUDE_DIR})
    SET(LIBFREERADIUS_LIBRARIES ${LIBFREERADIUS_LIBRARY})
ENDIF()
