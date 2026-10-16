cmake_minimum_required(VERSION 4.3)
project(rgl C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(rgl STATIC
        src/clear.c
        src/context.c
        src/state.c
)
target_include_directories(rgl
        PUBLIC include
        PRIVATE src
)

if (MSVC)
    target_compile_options(rgl PRIVATE /W4)
else ()
    target_compile_options(rgl PRIVATE -Wall -Wextra -Wpedantic)
endif ()

enable_testing()

add_executable(rgl_test_clear tests/test_clear.c)
target_link_libraries(rgl_test_clear PRIVATE rgl)
add_test(NAME clear COMMAND rgl_test_clear)
