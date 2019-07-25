
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

class SingleThreadWorker {
public:
	std::queue<int> clientConnections;
	
	SingleThreadWorker() {}
	
	size_t size() {
		return clientConnections.size();
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
