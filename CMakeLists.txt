cmake_minimum_required(VERSION 3.21)

project(luabind)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(LUABIND_LUA_LIB_NAME luabind_lua CACHE STRING "CMake name of lua library, luabind should be linked with.")
set(LUABIND_LUA_CPP OFF CACHE BOOL "Whether lua was compiled as C++ and headers should be included without extern 'C'.")

option(LUABIND_UNIT_TESTS "Enable unit tests." OFF)
option(LUABIND_BENCHMARKS "Enable benchmarks." OFF)
option(LUABIND_CODE_COVERAGE "Enable coverage reporting in tests" OFF)

if (LUABIND_UNIT_TESTS OR LUABIND_BENCHMARKS)
    set(LUABIND_TESTS ON)
else()
    set(LUABIND_TESTS OFF)
endif()

add_subdirectory(third_party)

add_library(luabind INTERFACE)
target_include_directories(luabind INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(luabind INTERFACE ${LUABIND_LUA_LIB_NAME})

if(LUABIND_LUA_CPP)
    add_compile_definitions(LUABIND_LUA_CPP)
    target_compile_definitions(luabind INTERFACE LUABIND_LUA_CPP)
endif(LUABIND_LUA_CPP)

if(LUABIND_TESTS)
    target_compile_options(luabind INTERFACE -Wall -Wextra -Wnewline-eof -Wformat -Werror)

    if(LUABIND_CODE_COVERAGE)
        # Add required flags (GCC & LLVM/Clang)
        target_compile_options(luabind INTERFACE
            -O0        # no optimization
            -g         # generate debug info
            --coverage # sets all required flags
        )
        target_link_options(luabind INTERFACE --coverage)
    endif(LUABIND_CODE_COVERAGE)

    enable_testing()
    if (LUABIND_UNIT_TESTS)
        add_subdirectory(tests)
    endif()
    if (LUABIND_BENCHMARKS)
        add_subdirectory(benchmarks)
    endif()
endif()
