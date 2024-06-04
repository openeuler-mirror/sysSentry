project(cpu_patrol C)

file(GLOB CPU_PATROL_SRC *.c)

add_library(cpu_patrol SHARED ${CPU_PATROL_SRC})

TARGET_LINK_LIBRARIES(cpu_patrol pthread -ldl)

