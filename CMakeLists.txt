cmake_minimum_required(VERSION 3.14)
project(TCP_Testing)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "-O2")

set(CMAKE_BUILD_TYPE debug)

set(DEBUG_OUTPUT off)
set(PARALLEL_SEND_AND_RECEIVE on)
set(SEND_RECEIVE_SEPARATE on)
set(EDGE_TRIGGERED off)

file(GLOB DATA data.h)
file(GLOB READ_WRITE_QUEUE atomicops.h readerwriterqueue.h)
file(GLOB RING_BUFFER common.h ring_buffer.hpp )

IF (DEBUG_OUTPUT)
    ADD_DEFINITIONS(-DDEBUG_OUTPUT)
ENDIF ()

IF (SEND_RECEIVE_SEPARATE)
    ADD_DEFINITIONS(-DSEND_RECEIVE_SEPARATE)
ENDIF ()

IF (PARALLEL_SEND_AND_RECEIVE)
    ADD_DEFINITIONS(-DPARALLEL_SEND_AND_RECEIVE)
ENDIF ()

IF (EDGE_TRIGGERED)
    ADD_DEFINITIONS(-DEDGE_TRIGGERED)
ENDIF ()

add_executable(server server.cpp ${DATA} worker.h)
TARGET_LINK_LIBRARIES(server pthread)

add_executable(epoll_server epoll_server.cpp ${DATA} ${READ_WRITE_QUEUE} mullti_thread_worker.h)
TARGET_LINK_LIBRARIES(epoll_server pthread)

add_executable(multi_thread_epoll_server multi_thread_epoll_server.cpp ${DATA} ${READ_WRITE_QUEUE} mullti_thread_worker.h)
TARGET_LINK_LIBRARIES(multi_thread_epoll_server pthread)

add_executable(client client.cpp client.h ${DATA})

add_executable(client_pool client_pool.cpp client.h ${DATA})
TARGET_LINK_LIBRARIES(client_pool pthread)

add_executable(epoll_server_example epoll_server_example.cpp ${DATA} mullti_thread_worker.h)
TARGET_LINK_LIBRARIES(epoll_server_example pthread)