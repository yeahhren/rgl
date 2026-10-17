cmake_minimum_required(VERSION 4.3)
project(rgl C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(rgl STATIC
        src/buffer.c
        src/clear.c
        src/clip.c
        src/context.c
        src/current.c
        src/fragment.c
        src/light.c
        src/line.c
        src/matrix.c
        src/point.c
        src/polygon.c
        src/primitive.c
        src/state.c
        src/vertex.c
)
target_include_directories(rgl
        PUBLIC include
        PRIVATE src
)

if (NOT WIN32)
    target_link_libraries(rgl PRIVATE m)
endif ()

if (MSVC)
    target_compile_options(rgl PRIVATE /W4)
else ()
    target_compile_options(rgl PRIVATE -Wall -Wextra -Wpedantic)
endif ()

add_executable(rgl_test test.c)
target_link_libraries(rgl_test PRIVATE rgl)
