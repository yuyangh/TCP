# TCP Testing Platform

## Introduction
A TCP Testing platform which uses epoll api and libevent library. 

## Contents

### Shared Files

data.h  
define the file format transmitted between the server and the client

### Server
multi_queue_epoll_server.cpp  
one epoll instance, each worker thread has its own concurrent queue

single_queue_epoll_server.cpp  
one epoll instance, all worker threads share a single concurrent queue

libevent_multi_worker.cpp  
one libevent base leader, and each worker thread has a libevent base follower

### Client
client.h  
define prototypes for client

client_pool.cpp  
creates **NUM_THREADS**, each thread will do **NUM_TASK_PER_THREAD** tasks. For each task, it is a send and receive 

single_client.cpp  
a single client that do single send and receive

## Compilation Flags
### DEBUG_OUTPUT
This mode will print many information.   
At the same time, it will check whether Package object sent has an identical value id as the received Result object's id. This is only for debug purpose.   
In other word, suppose we sent Package package, and receive Result result:

    // there is a Package object and a Result object
    // the test method in client.h will check following
    package.value.id == result.id


### PROFILING 
will integrate gprof to do profiling, may need to enable the flag "WASTE_TIME" to help do profiling

### experimental flag
PARALLEL_SEND_AND_RECEIVE  
SEND_RECEIVE_SEPARATE  
EDGE_TRIGGERED  

## Performance 
In release mode, client and server bound on same cpunodebind,

    NUM_WORKERS = 20
    NUM_THREADS = 1000
    NUM_TASK_PER_THREAD = 1000

the performance is： 

| Executable file      | QPS | 
| :---        |    ----:   | 
| ./libevent_multi_worker      | 1,689,212       | 
| ./single_queue_epoll_server   | 250,055        | 
| ./multi_queue_epoll_server   | 258,632        | 

## 