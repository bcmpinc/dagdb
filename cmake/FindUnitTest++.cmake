# Locate UNITTEST
# This module defines
# UNITTEST++_LIBRARY
# UNITTEST++_FOUND, if false, do not try to link to gdal 
# UNITTEST++_INCLUDE_DIR, where to find the headers
#

FIND_PATH(UNITTEST++_INCLUDE_DIR UnitTest++.h
    ${UNITTEST++_DIR}/include
    $ENV{UNITTEST++_DIR}/include
    $ENV{UNITTEST++_DIR}
    ~/Library/Frameworks
    /Library/Frameworks
    /usr/local/include
    /usr/include
    /usr/include/unittest++
    /sw/include # Fink
    /opt/local/include # DarwinPorts
    /opt/csw/include # Blastwave
    /opt/include
    [HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session\ Manager\\Environment]/include
    /usr/freeware/include
)

FIND_LIBRARY(UNITTEST++_LIBRARY 
    NAMES unittest++
    PATHS
    ${UNITTEST++_DIR}/lib
    $ENV{UNITTEST++_DIR}/lib
    $ENV{UNITTEST++_DIR}
    ~/Library/Frameworks
    /Library/Frameworks
    /usr/local/lib
    /usr/lib
    /sw/lib
    /opt/local/lib
    /opt/csw/lib
    /opt/lib
    [HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session\ Manager\\Environment]/lib
    /usr/freeware/lib64
)

SET(UNITTEST++_FOUND "NO")
IF(UNITTEST++_LIBRARY AND UNITTEST++_INCLUDE_DIR)
    SET(UNITTEST++_FOUND "YES")
    MESSAGE(STATUS "Found unittest++: '${UNITTEST++_LIBRARY}' and header in '${UNITTEST++_INCLUDE_DIR}'")
ENDIF(UNITTEST++_LIBRARY AND UNITTEST++_INCLUDE_DIR)

MARK_AS_ADVANCED(
    UNITTEST++_LIBRARY
    UNITTEST++_INCLUDE_DIR
)
