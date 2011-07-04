cmake_minimum_required (VERSION 2.6)
project(dagdb)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

find_package(Libgcrypt)

set(lib_src
    src/base.cpp
)

set(test_src
    src/test.cpp
)

add_library(dagdb SHARED ${lib_src})
target_link_libraries(dagdb ${LIBGCRYPT_LIBRARIES})
set_property(TARGET dagdb PROPERTY COMPILE_FLAGS "${LIBGCRYPT_CFLAGS}")

add_executable(dagdb_test ${test_src}) 
target_link_libraries(dagdb_test dagdb)

