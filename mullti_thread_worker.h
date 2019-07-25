
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
#include "ring_buffer.hpp"

class MultiThreadWorker {
public:
	/**
	 * a wrapper for the RingBuffer class
	 * @param length set the length for the ring_buffer
	 */
	MultiThreadWorker(size_t length = 1000) : ring_buffer(length) {}
	
	inline void addJob(struct epoll_event &job) {
		ring_buffer.Push(job);
	}
	
	/**
	 * get and pop the the job from the queue
	 * @return
	 */
	inline struct epoll_event PopJob() {
		return ring_buffer.Pop();
	}
	
	size_t Size() {
		return ring_buffer.Length();
	}

private:
	RingBuffer<struct epoll_event, true, true> ring_buffer;
};

#endif //TCPEXAMPLE_MULLTI_THREAD_WORKER_H
