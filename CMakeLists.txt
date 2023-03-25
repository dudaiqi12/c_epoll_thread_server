cmake_minimum_required(VERSION 2.6)

project(C_EPOLL_SERVER)

include_directories(${CMAKE_SOURCE_DIR}/include)

add_compile_options("-Wall")

set(CMAKE_BUILD_TYPE "Debug")

set(SRC
 src/server.c)
 
add_executable(../main src/main.c ${SRC})
