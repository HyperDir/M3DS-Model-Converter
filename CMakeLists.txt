cmake_minimum_required(VERSION 4.2)
project(M3DSModelConverter)

set(CMAKE_CXX_STANDARD 26)

file(GLOB_RECURSE HEADER_FILES
        "include/*.h"
        "include/*.hpp"
        "include/*.hxx"
)

file(GLOB_RECURSE SOURCE_FILES
        "source/*.c"
        "source/*.cpp"
        "source/*.cxx"
)

add_executable(M3DSModelConverter
        ${HEADER_FILES}
        ${SOURCE_FILES}
)

target_include_directories(M3DSModelConverter PRIVATE include)

target_compile_options(M3DSModelConverter PRIVATE
        -O2
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -fcontract-evaluation-semantic=enforce
)

target_link_libraries(M3DSModelConverter PRIVATE
        stdc++exp
)
