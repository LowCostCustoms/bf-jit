cmake_minimum_required(VERSION 3.20)
project(bfjit)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(mir)

find_package(fmt REQUIRED)

set(HEADERS
        src/bfjit/exception.hpp
        src/bfjit/instruction.hpp
        src/bfjit/types.hpp
        src/bfjit/mir_compiler.hpp
        src/bfjit/arguments.hpp)

set(SOURCES
        src/bfjit/bfjit.cpp
        src/bfjit/mir_compiler.cpp
        src/bfjit/exception.cpp)

add_executable(bfjit ${SOURCES} ${HEADERS})
target_link_libraries(bfjit PUBLIC mir fmt)
target_include_directories(bfjit PUBLIC src)
target_include_directories(bfjit PRIVATE mir)
