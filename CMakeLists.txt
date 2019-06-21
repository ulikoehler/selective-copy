cmake_minimum_required(VERSION 3.0)
include(CheckSymbolExists)

add_library(openlog SHARED
    main.c
)
target_link_libraries(openlog dl)

list(APPEND CMAKE_REQUIRED_LIBRARIES m)
check_symbol_exists(open64 fcntl.h HAVE_OPEN64)

if(HAVE_OPEN64)
    target_compile_definitions(openlog PRIVATE -DHAVE_OPEN64)
endif()
