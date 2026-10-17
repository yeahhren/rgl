cmake_minimum_required(VERSION 4.3)
project(rgl C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(rgl STATIC
        src/accum.c
        src/buffer.c
        src/clear.c
        src/clip.c
        src/context.c
        src/current.c
        src/drawpix.c
        src/eval.c
        src/fog.c
        src/fragment.c
        src/light.c
        src/list.c
        src/line.c
        src/matrix.c
        src/pixel.c
        src/point.c
        src/polygon.c
        src/primitive.c
        src/raster.c
        src/state.c
        src/texgen.c
        src/texture.c
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

add_executable(rgl_test test.c png.c)
target_link_libraries(rgl_test PRIVATE rgl)
target_compile_definitions(rgl_test PRIVATE RGL_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")
