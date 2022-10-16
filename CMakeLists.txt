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

add_library(q2tools-i INTERFACE)
target_compile_options(q2tools-i INTERFACE -fno-common -Wall -Wno-unused-result -Wno-strict-aliasing ${THREADING_OPTION} -DUSE_ZLIB)
target_link_libraries(q2tools-i INTERFACE m ${THREADING_LIB})

add_library(map2-lib STATIC
    common/bspfile.c
    common/cmdlib.c
    common/l3dslib.c
    common/lbmlib.c
    common/llwolib.c
    common/mathlib.c
    common/mdfour.c
    common/polylib.c
    common/scriplib.c
    common/threads.c
    common/trilib.c
    map2/bsp.c
    map2/brushbsp.c
    map2/csg.c
    map2/faces.c
    map2/leakfile.c
    map2/map.c
    map2/portals.c
    map2/prtfile.c
    map2/textures.c
    map2/tree.c
    map2/writebsp.c

    map2/vis.c
    map2/flow.c

    map2/rad.c
    map2/lightmap.c
    map2/patches.c
    map2/trace.c
)
target_include_directories(map2-lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/common)

add_executable(map2 map2/main.c)

add_executable(data2
    data2/data2.c
    data2/images.c
    data2/models.c
    data2/sprites.c
    data2/tables.c
    data2/video.c
)

target_link_libraries(map2 q2tools-i map2-lib)
target_link_libraries(data2 q2tools-i map2-lib)
