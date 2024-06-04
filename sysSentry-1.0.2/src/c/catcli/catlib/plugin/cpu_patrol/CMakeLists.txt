project(cpu_patrol C)

file(GLOB CPU_PATROL_SRC *.c)

add_library(cpu_patrol SHARED ${CPU_PATROL_SRC})

include_directories(${CPU_PATROL_SRC}/../../libso/)
link_directories(${CPU_PATROL_SRC}/../../libso/build/xalarm/)

TARGET_LINK_LIBRARIES(cpu_patrol pthread -ldl ${CPU_PATROL_SRC}/../../libso/build/xalarm/libxalarm.so)

