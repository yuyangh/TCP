#include<netinet/in.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <unistd.h>
#include <vector>
#include <iostream>
#include <thread>
#include <sys/epoll.h>
#include <arpa/inet.h>

#include "data.h"
#include "worker.h"


#define LENGTH_OF_SERVER_LISTEN_QUEUE     10000
#define NUM_WORKERS 20
static std::vector<SingleThreadWorker> works(NUM_WORKERS);

void Process(int);

void Work(int i) {
	while (true) {
		int socket = works[i].getConnection();
		if (socket == -1) {
			continue;
		} else {
			Process(socket);
		}
	}
}

void Process(int client_socket) {
	// receive the Package from the client
	char recvPackage[PACKAGE_BUFFER_SIZE];
	Package buffer;
	memset(&buffer, 0, sizeof(Package)); // clean to 0
	ssize_t rs = recv(client_socket, recvPackage, PACKAGE_BUFFER_SIZE, 0);
	memcpy(&buffer, recvPackage, sizeof(Package));
#ifdef DEBUG_OUTPUT
	printf("recv over id:%u, key:%d, value:%d\n", (unsigned int) buffer.id, buffer.key.id, buffer.value.id);
#endif
	
	// send the response
	int sendbytes;
	Result result(buffer.id);
	if ((sendbytes = send(client_socket, (char *) &result, sizeof(Result), 0)) == -1) {
		perror("respond");
		exit(1);
	}
#ifdef DEBUG_OUTPUT
	printf("send the Result, id:%d\n", result.id);
#endif
	close(client_socket);
}

void InitializeWorkers(std::vector<std::thread> &workers) {
	for (int i = 0; i < NUM_WORKERS; ++i) {
		workers.push_back(std::thread(Work, i));
		if (workers[i].joinable()) {
			workers[i].detach();
		}
	}
}

void polling(std::vector<std::thread> &workers, int64_t count, int connection) {
	works[count % NUM_WORKERS].addConnections(connection);
}

int main(int argc, char **argv) {
	
	// set socket's address information
	struct sockaddr_in server_addr;
	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htons(INADDR_ANY);
	int server_port = SERVER_PORT;
	if (argc > 1) {
		server_port = atoi(argv[1]);
	}
	server_addr.sin_port = htons(server_port);
	
	// create a stream socket
	int server_fd = socket(PF_INET, SOCK_STREAM, 0);
	if (server_fd < 0) {
		printf("Create Socket Failed!\n");
		exit(1);
	}
	
	//bind
	if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr))) {
		printf("Server Bind Port: %d Failed!\n", server_port);
		exit(1);
	}
	
	// listen
	if (listen(server_fd, LENGTH_OF_SERVER_LISTEN_QUEUE)) {
		printf("Server Listen Failed!\n");
		exit(1);
	}
	// initializeWorkers
	std::vector<std::thread> workers(NUM_WORKERS);
	InitializeWorkers(workers);
	int64_t count = 0;
	
	while (true) {
		struct sockaddr_in client_addr;
		socklen_t length = sizeof(client_addr);
		
		int new_client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &length);
		if (new_client_fd < 0) {
			printf("Server Accept Failed!\n");
			break;
		}
		
		// multithreading version
		// distribute each FD's activity to one of the thread
		polling(workers, count, new_client_fd);
		count++;
		
		
		// if uncomment the following part,
		// the server will do single thread send and receive
		
		/*
		
		// receive the Package from the client
		char recvPackage[PACKAGE_BUFFER_SIZE];
		Package buffer;
		memset(&buffer, 0, sizeof(Package)); // clean to 0
		ssize_t rs = recv(new_client_fd, recvPackage, PACKAGE_BUFFER_SIZE, 0);
		memcpy(&buffer, recvPackage, sizeof(Package));
		printf("recv over id:%u, key:%d, value:%d\n", (unsigned int) buffer.id, buffer.key.id, buffer.value.id);
		
		// send the response
		int sendbytes;
		Result result(buffer.id);
		if ((sendbytes = send(new_client_fd, (char *) &result, sizeof(Result), 0)) == -1) {
			perror("respond");
			exit(1);
		}
		printf("send the Result, id:%d\n",result.id);
		close(new_client_fd);
		*/
	}
	close(server_fd);
	
	return 0;
}