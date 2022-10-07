cmake_minimum_required(VERSION 3.11)
project(q2tools-220 LANGUAGES C)


set(Q2T_USE_PTHREADS ON CACHE BOOL "compile with pthread support")

if(Q2T_USE_PTHREADS)
  set(THREADING_OPTION -pthread -DUSE_PTHREADS)
else()
  set(THREADING_OPTION -DUSE_SETRLIMIT)
endif()

add_library(q2tools-i INTERFACE)
target_compile_options(q2tools-i INTERFACE -fno-common -Wall -Wno-unused-result -Wno-strict-aliasing ${THREADING_OPTION} -DUSE_ZLIB)
target_link_libraries(q2tools-i INTERFACE m)

add_library(q2tools-220-lib STATIC
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
    4bsp/4bsp.c
    4bsp/brushbsp.c
    4bsp/csg.c
    4bsp/faces.c
    4bsp/leakfile.c
    4bsp/map.c
    4bsp/portals.c
    4bsp/prtfile.c
    4bsp/textures.c
    4bsp/tree.c
    4bsp/writebsp.c

    4vis/4vis.c
    4vis/flow.c

    4rad/4rad.c
    4rad/lightmap.c
    4rad/patches.c
    4rad/trace.c
)
target_include_directories(q2tools-220-lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/common)

add_executable(q2tools-220 main.c)
add_executable(4bsp 4bsp/4bsp_main.c)
add_executable(4vis 4vis/4vis_main.c)
add_executable(4rad 4rad/4rad_main.c)
add_executable(4data
    4data/4data.c
    4data/images.c
    4data/models.c
    4data/sprites.c
    4data/tables.c
    4data/video.c
)

target_link_libraries(q2tools-220 q2tools-i q2tools-220-lib)
target_link_libraries(4bsp q2tools-i q2tools-220-lib)
target_link_libraries(4vis q2tools-i q2tools-220-lib)
target_link_libraries(4rad q2tools-i q2tools-220-lib)
target_link_libraries(4data q2tools-i q2tools-220-lib)
