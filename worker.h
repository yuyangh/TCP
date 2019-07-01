//
// Created by yuyanghu on 6/11/2019.
//

#ifndef TCPEXAMPLE_WORKER_H
#define TCPEXAMPLE_WORKER_H

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

#define MAX_QUEUE_SIZE 100

class Worker {
public:
	std::queue<int> clientConnections;
	moodycamel::ReaderWriterQueue<struct epoll_event> buffer;
	
	Worker():buffer(MAX_QUEUE_SIZE) {}
	
	inline void addJob(struct epoll_event &event){
#ifdef DEBUG_OUTPUT
		printf("buffer size before addJob:\t %d\n",(int)buffer.size_approx());
#endif
		buffer.enqueue(std::move(event));
#ifdef DEBUG_OUTPUT
		printf("buffer size after addJob:\t %d\n",(int)buffer.size_approx());
#endif
	}
	
	struct epoll_event* getJob(){
		return buffer.peek();
	}
	
	void popJob(){
#ifdef DEBUG_OUTPUT
		printf("buffer size before popJob:\t %d\n",(int)buffer.size_approx());
#endif
		buffer.pop();
#ifdef DEBUG_OUTPUT
		printf("buffer size after popJob:\t %d\n",(int)buffer.size_approx());
#endif
	}

	size_t size_approx(){
		return buffer.size_approx();
	}
	
	// deprecated
	void addConnections(int socket) {
		clientConnections.push(socket);
	}
	
	/**
	 * deprecated method
	 * @return -1 if empty
	 */
	int getConnection() {
		if (clientConnections.empty()) {
			return -1;
		} else {
			int socket = clientConnections.front();
			clientConnections.pop();
			return socket;
		}
	}

	bool empty() {
		return clientConnections.empty();
	}
	
};

#endif //TCPEXAMPLE_WORKER_H
