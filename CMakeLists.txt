cmake_minimum_required(VERSION 3.0)

# Project settings
project(mcsv CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler checks
set(CLANG FALSE)
set(GCC FALSE)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CLANG TRUE)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(GCC TRUE)
endif()

# Warnings
include(cmake/compiler_warnings.cmake)
if(MSVC)
    add_compile_options(${MSVC_WARNINGS})
elseif(${CLANG})
    add_compile_options(${CLANG_WARNINGS})
elseif(${GCC})
    add_compile_options(${GCC_WARNINGS})
endif()

# Sanitizers enabled on Debug-mode
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND (${GCC} OR ${CLANG}) )
    add_compile_options("-fsanitize=address,leak,undefined")
    add_link_options("-fsanitize=address,leak,undefined")
endif()

# Header-only library
add_library(mcsv INTERFACE)
target_include_directories(mcsv INTERFACE ${CMAKE_SOURCE_DIR}/include)
set_target_properties(mcsv PROPERTIES PUBLIC_HEADER ${CMAKE_SOURCE_DIR}/include/mcsv/mcsv.hpp)
install(TARGETS mcsv PUBLIC_HEADER DESTINATION include/mcsv)

# Test program
add_executable(test ${CMAKE_SOURCE_DIR}/test/test.cpp)
target_link_libraries(test mcsv)
configure_file(test/test.csv test.csv COPYONLY)

