
#ifndef TCPEXAMPLE_MULLTI_THREAD_WORKER_H
#define TCPEXAMPLE_MULLTI_THREAD_WORKER_H

#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vector>
#include <queue>

#include <iostream>
#include <thread>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <iostream>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <csignal>
#include "data.h"
#include "readerwriterqueue.h"
#include "ring_buffer.hpp"

static const size_t MAX_QUEUE_SIZE=100;

class Worker {
public:
	
	Worker():ring_buffer(MAX_QUEUE_SIZE) {}
	
	inline void addJob(struct epoll_event &event){
		ring_buffer.Push((event));
	}
	
	struct epoll_event PopJob(){
		return ring_buffer.Pop();
	}

private:
	CarpLog::RingBuffer<struct epoll_event> ring_buffer;
};

#endif //TCPEXAMPLE_MULLTI_THREAD_WORKER_H
