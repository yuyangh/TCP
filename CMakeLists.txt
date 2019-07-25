cmake_minimum_required(VERSION 3.14)
project(TCP_Testing)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "-O3")

set(CMAKE_BUILD_TYPE debug)

set(PROFILING on)
set(DEBUG_OUTPUT off)
set(PARALLEL_SEND_AND_RECEIVE on)
set(SEND_RECEIVE_SEPARATE on)
set(EDGE_TRIGGERED off)

file(GLOB DATA data.h)
file(GLOB READ_WRITE_QUEUE atomicops.h readerwriterqueue.h)
file(GLOB RING_BUFFER common.h ring_buffer.hpp )

IF (PROFILING)
    set(CMAKE_CXX_FLAGS "-pg")
    set(LINK_FLAGS "-pg")
    SET(CMAKE_SHARED_LINKER_FLAGS "-pg")
ENDIF ()

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

add_executable(basic_server.cpp ${DATA} worker.h)
TARGET_LINK_LIBRARIES(basic_server pthread)

add_executable(single_thread_libevent_server.cpp ${DATA} worker.h)
TARGET_LINK_LIBRARIES(single_thread_libevent_server pthread)
TARGET_LINK_LIBRARIES(single_thread_libevent_server event_core)

add_executable(multi_thread_libevent_server.cpp ${DATA} worker.h)
TARGET_LINK_LIBRARIES(multi_thread_libevent_server pthread)
TARGET_LINK_LIBRARIES(multi_thread_libevent_server event_core)
TARGET_LINK_LIBRARIES(multi_thread_libevent_server event_pthreads)

add_executable(libevent_multi_worker.cpp ${DATA} worker.h)
TARGET_LINK_LIBRARIES(libevent_multi_worker pthread)
TARGET_LINK_LIBRARIES(libevent_multi_worker event_core)
TARGET_LINK_LIBRARIES(libevent_multi_worker event_pthreads)

add_executable(libevent_server_ref.cpp ${DATA} worker.h)
TARGET_LINK_LIBRARIES(libevent_server_ref pthread)
TARGET_LINK_LIBRARIES(libevent_server_ref event_core)
TARGET_LINK_LIBRARIES(libevent_multi_worker event_pthreads)

add_executable(multi_queue_epoll_server.cpp ${DATA} ${READ_WRITE_QUEUE} mullti_thread_worker.h)
TARGET_LINK_LIBRARIES(multi_queue_epoll_server pthread)

add_executable(single_queue_epoll_server.cpp ${DATA} ${READ_WRITE_QUEUE} mullti_thread_worker.h)
TARGET_LINK_LIBRARIES(single_queue_epoll_server pthread)

add_executable(single_client.cpp client.h ${DATA})

add_executable(client_pool.cpp client.h ${DATA})
TARGET_LINK_LIBRARIES(client_pool pthread)

add_executable(epoll_server_example.cpp ${DATA} mullti_thread_worker.h)
TARGET_LINK_LIBRARIES(epoll_server_example pthread)