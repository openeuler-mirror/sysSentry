project(cpu_patrol C)

file(GLOB CPU_PATROL_SRC *.c)

add_library(cpu_patrol SHARED ${CPU_PATROL_SRC})

include_directories(${CMAKE_SOURCE_DIR}/../../../libso/)
link_directories(${CMAKE_SOURCE_DIR}/../../../libso/build/xalarm/)

TARGET_LINK_LIBRARIES(cpu_patrol pthread -ldl ${CMAKE_SOURCE_DIR}/../../../libso/build/xalarm/libxalarm.so)

