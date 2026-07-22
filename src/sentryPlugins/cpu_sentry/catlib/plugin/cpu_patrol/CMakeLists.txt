project(cpu_patrol C)

set(CMAKE_SKIP_RPATH TRUE)

file(GLOB CPU_PATROL_SRC *.c)

add_library(cpu_patrol SHARED ${CPU_PATROL_SRC})

include_directories(${CMAKE_SOURCE_DIR}/../../../libs/libxalarm/)
include_directories(${CMAKE_SOURCE_DIR}/../../../libsentry/c/io/)
link_directories(${CMAKE_SOURCE_DIR}/../../../libs/build/libxalarm/)

TARGET_LINK_LIBRARIES(cpu_patrol pthread xalarm ${CMAKE_SOURCE_DIR}/../../../libsentry/c/io/build/libsentry_io.so -ldl)

set_target_properties(cpu_patrol PROPERTIES
    COMPILE_FLAGS "-fPIC -fstack-protector-strong -D_FORTIFY_SOURCE=2 -O2 -Wall -Wtrampolines -g"
    LINK_FLAGS "-Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack"
)
