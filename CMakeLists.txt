cmake_minimum_required(VERSION 3.0)

# Project settings
project(mycsv CXX)
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

# Executable
add_executable(${PROJECT_NAME} main.cpp)

# Install settings
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
