cmake_minimum_required(VERSION 3.11)
project(q2tools-220 LANGUAGES C)


set(Q2T_USE_PTHREADS ON CACHE BOOL "compile with pthread support")

if(Q2T_USE_PTHREADS)
  set(THREADING_OPTION -pthread -DUSE_PTHREADS)
	set(THREADING_LIB pthread)
else()
  set(THREADING_OPTION -DUSE_SETRLIMIT)
	set(THREADING_LIB)
endif()

set(CMAKE_STATIC_LIBRARY_PREFIX_C "")

add_library(q2tools-i INTERFACE)
target_compile_options(q2tools-i INTERFACE -fno-common -Wall -Wimplicit-function-declaration -Wno-strict-aliasing ${THREADING_OPTION} -DUSE_ZLIB -O3 -s -z stack-size=16777216)
target_link_libraries(q2tools-i INTERFACE m ${THREADING_LIB})

add_library(libq2tool STATIC
    src/bspfile.c
    src/cmdlib.c
    src/l3dslib.c
    src/lbmlib.c
    src/llwolib.c
    src/mathlib.c
    src/mdfour.c
    src/polylib.c
    src/scriplib.c
    src/threads.c
    src/trilib.c
    src/bsp.c
    src/brushbsp.c
    src/csg.c
    src/faces.c
    src/leakfile.c
    src/map.c
    src/portals.c
    src/prtfile.c
    src/textures.c
    src/tree.c
    src/writebsp.c

    src/vis.c
    src/flow.c

    src/rad.c
    src/lightmap.c
    src/patches.c
    src/trace.c

    src/data.c
    src/images.c
    src/models.c
    src/sprites.c
    src/tables.c
    src/video.c
)
target_include_directories(libq2tool PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/common)

add_executable(q2tool src/main.c)


target_link_libraries(q2tool q2tools-i libq2tool)