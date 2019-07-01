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
#include <unordered_map>

#include "data.h"
#include "worker.h"

using namespace std;
#define NUM_WORKERS             20
#define ECHO_SERVER_PORT        6000
#define NUM_FD                  1200
#define LISTEN_BACKLOG          16
#define MAX_EPOLL_EVENT_COUNT   (NUM_FD>>2)
#define EPOLL_WAIT_TIMEOUT      -1


static std::vector<Worker> works(NUM_WORKERS);
static int epoll_fd, server_fd;
static volatile bool running = true;
// event for the server
static struct epoll_event event;

static unsigned long long JobCount = 0;
static unordered_map<int, Package> ClientInfoMap;
static unordered_map<int, ProcessStatus> FDProcessStatus;
// todo may optimize unordered_map to array to accelerate
static ProcessStatus FDProcessStatusArr[NUM_FD];


void process(struct epoll_event *epollEvent);

void ProgramTerminated();

void ReceivePackage(struct epoll_event *epollEvent);

void SendResult(struct epoll_event *epollEvent);

void sig_handler(int sig) {
	if (sig == SIGINT) {
		ProgramTerminated();
	}
}

void InitializeFDProcessStatusArr(){
	for (int i = 0; i <NUM_FD ; ++i) {
		FDProcessStatusArr[i]=ProcessStatus();
	}
}

// back end thread
void ProcessJob(int i) {
	while (true) {
		struct epoll_event *epollEvent = works[i].getJob();
		if (epollEvent == nullptr) {
			continue;
		}

#ifdef DEBUG_OUTPUT
		printf("size of the queues[%d]: %d \n", i, (int) works[i].size_approx());
#endif
		
		if ((epollEvent->events) & EPOLLIN) {
			// receive from the client
			ReceivePackage(epollEvent);
			works[i].popJob();
		} else {
			if ((epollEvent->events) & EPOLLOUT) {
				// send to the client
				SendResult(epollEvent);
				works[i].popJob();
			} else {
				assert(false);
			}
		}
	}
}

// current version
void ReceivePackage(struct epoll_event *epollEvent) {
	
	int epollEventFD = epollEvent->data.fd;
	// in receiving, so not in ready stage
	assert(!FDProcessStatus[epollEventFD].ready_to_receive_);
	
	// receive the Package from the client
	Package buffer;
	char recvPackage[PACKAGE_BUFFER_SIZE];
	// Package buffer;
	memset(&buffer, 0, sizeof(Package)); // clean to 0
	int ret = recv(epollEventFD, recvPackage, PACKAGE_BUFFER_SIZE, 0);
	if (ret <= 0) {
#ifdef DEBUG_OUTPUT
		printf("close connection from fd: %d \n", epollEventFD);
#endif
		// delete the event, last argument can be ignored
		epoll_ctl(epoll_fd, EPOLL_CTL_DEL, epollEventFD, epollEvent);
		
		// close fd
		close(epollEventFD);
		
		// update ready_to_send_ and ready_to_receive_ status
		FDProcessStatus[epollEventFD].ready_to_send_ = true;
		FDProcessStatus[epollEventFD].ready_to_receive_ = true;
		return;
	} else {
		memcpy(&buffer, recvPackage, sizeof(Package));
#ifdef DEBUG_OUTPUT
		printf("fd: %d \t recv over id:%u, key:%d, value:%d\n", epollEventFD, (unsigned int) buffer.id,
			   buffer.key.id, buffer.value.id);
#endif
	}


#ifdef EDGE_TRIGGERED
	epollEvent->events=EPOLLOUT | EPOLLET;
#else
	epollEvent->events = EPOLLOUT;
#endif
	
	// store the received information
	ClientInfoMap[epollEventFD] = (buffer);

#ifdef DEBUG_OUTPUT
	printf("Store clientInfo: fd: %d, value:%d \n", epollEventFD, buffer.value.id);
#endif
	
	epoll_ctl(epoll_fd, EPOLL_CTL_MOD, epollEventFD, epollEvent);
	
	// receive done, able to send
	FDProcessStatus[epollEventFD].ready_to_send_ = true;
}

// current version
void SendResult(struct epoll_event *epollEvent) {
	
	int epollEventFD = epollEvent->data.fd;
	// in sending stage, so this fd is no longer in ready stage
	assert(!FDProcessStatus[epollEventFD].ready_to_send_);
	
	// send the response
#ifdef DEBUG_OUTPUT
	printf("Read ClientInfoMap: fd: %d, value:%d \n", epollEventFD,
		   ClientInfoMap[epollEventFD].value.id);
#endif
	
	Result result(ClientInfoMap[epollEventFD].value.id);
	int sendbytes = send(epollEventFD, (char *) &result, sizeof(Result), 0);
	if (sendbytes < 0) {
		perror("send failed.\n");
		return;
	}

#ifdef EDGE_TRIGGERED
	epollEvent->events=EPOLLIN | EPOLLET;
#else
	epollEvent->events = EPOLLIN;
#endif

#ifdef DEBUG_OUTPUT
	printf("fd: %d \t send the Result, id:%d\n", epollEventFD, result.id);
#endif
	
	epoll_ctl(epoll_fd, EPOLL_CTL_MOD, epollEventFD, epollEvent);
	
	// already send, so able to receive
	FDProcessStatus[epollEventFD].ready_to_receive_ = true;
}

// receive and send together
void process(struct epoll_event *epollEvent) {
	// receive the Package from the client
	Package buffer;
	char recvPackage[PACKAGE_BUFFER_SIZE];
	// Package buffer;
	memset(&buffer, 0, sizeof(Package)); // clean to 0
	int ret = recv(epollEvent->data.fd, recvPackage, PACKAGE_BUFFER_SIZE, 0);
	if (ret <= 0) {
#ifdef DEBUG_OUTPUT
		printf("close connection from fd: %d \n", epollEvent->data.fd);
#endif
		close(epollEvent->data.fd);
		epoll_ctl(epoll_fd, EPOLL_CTL_DEL, epollEvent->data.fd, &event);
		return;
	} else {
#ifdef DEBUG_OUTPUT
		printf("recv over id:%u, key:%d, value:%d\n", (unsigned int) buffer.id, buffer.key.id, buffer.value.id);
#endif
	}
	memcpy(&buffer, recvPackage, sizeof(Package));
	// packages[i] = (buffer);// may use std::move
	
	Result result(buffer.id);
	int sendbytes = send(epollEvent->data.fd, (char *) &result, sizeof(Result), 0);
	if (sendbytes < 0) {
		perror("send failed.\n");
		return;
	}
#ifdef DEBUG_OUTPUT
	printf("send the Result, id:%d\n", result.id);
#endif
}


void InitializeWorkers(std::vector<std::thread> &workers) {
	for (int i = 0; i < NUM_WORKERS; ++i) {
		workers.push_back(std::thread(ProcessJob, i));
		if (workers[i].joinable()) {
			workers[i].detach();
		}
	}
}

// todo may optimzie as pass by reference
inline void Polling(struct epoll_event epollEvent) {
	works[JobCount % NUM_WORKERS].addJob(epollEvent);
#ifdef DEBUG_OUTPUT
	printf("jobCount: %d \n", JobCount);
#endif
	++JobCount;
}

void SetNonBlocking(int sock) {
	int opts;
	opts = fcntl(sock, F_GETFL);
	if (opts < 0) {
		perror("fcntl(sock,GETFL)");
		exit(1);
	}
	opts = opts | O_NONBLOCK;
	if (fcntl(sock, F_SETFL, opts) < 0) {
		perror("fcntl(sock,SETFL,opts)");
		exit(1);
	}
}

void ProgramTerminated() {
	running = false;
	cout << "\n\nterminating the program..." << endl;
	close(epoll_fd);
	close(server_fd);
}


int main(int argc, char *argv[]) {
	printf("start epoll_server.cpp\n");
	signal(SIGINT, sig_handler);
	ClientInfoMap.reserve(NUM_FD);
	FDProcessStatus.reserve(NUM_FD);
	
	int i, maxi, server_fd, new_client_fd, sockfd, nfds/*number of fd s*/, portnumber;
	ssize_t n;
	socklen_t length;
	
	if (argc == 2) {
		if ((portnumber = atoi(argv[1])) < 0) {
			fprintf(stderr, "Usage:%s portnumber/a/n", argv[0]);
			return 1;
		}
	} else {
		portnumber = SERVER_PORT;
	}
	
	// declere epoll_event event_array as an array to deal with callback events
	struct epoll_event event_array[MAX_EPOLL_EVENT_COUNT];
	
	
	// creat the epoll_fd to deal with accept
	epoll_fd = epoll_create(NUM_FD);
	
	struct sockaddr_in client_addr;// todo
	struct sockaddr_in serveraddr;
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	
	// set socket to non-blocking
	SetNonBlocking(server_fd);
	
	//设置与要处理的事件相关的文件描述符
	event.data.fd = server_fd;
	//设置要处理的事件类型
	
	// EPOLLET is edge triggerd, default setting is level triggered
#ifdef EDGE_TRIGGERED
	event.events = EPOLLIN | EPOLLET;
#else
	event.events = EPOLLIN;
#endif
	
	// register epoll event
	// 1st: epoll_create()'s return value, 
	// 2nd: add new fd into epoll_fd
	// 3rd: fd needs to listen
	// 4th: what needs to listen
	
	// This system call is used to add, modify, or remove entries in the
	// interest list of the epoll(7) instance referred to by the file
	// descriptor epfd.  It requests that the operation op be performed for
	// the target file descriptor, fd.
	epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event);
	bzero(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htons(INADDR_ANY);
	// string local_addr = "127.0.0.1";
	// inet_aton(local_addr.c_str(), &(serveraddr.sin_addr));
	
	serveraddr.sin_port = htons(portnumber);
	bind(server_fd, (sockaddr * ) & serveraddr, sizeof(serveraddr));
	listen(server_fd, MAX_EPOLL_EVENT_COUNT);
	
	vector<Package> packages(MAX_EPOLL_EVENT_COUNT);
	
	// initializeWorkers thread to work
	std::vector<std::thread> workers(NUM_WORKERS);
	InitializeWorkers(workers);
	
	int nfds_old = 0;
	
	while (running) {
		// waiting for epoll to happen
		// interface: int epoll_wait(int epoll_fd, struct epoll_event *event_array, int maxevents, int timeout);
		
		// The epoll_wait() system call waits for events on the epoll(7)
		// instance referred to by the file descriptor epfd.
		// The memory area pointed to by events will contain
		// the events that will be available for the caller.
		// Up to maxevents are returned by epoll_wait().
		// The maxevents argument must be greater than zero.
		nfds = epoll_wait(epoll_fd, event_array, MAX_EPOLL_EVENT_COUNT, EPOLL_WAIT_TIMEOUT);
		
		// handle all happeing event_array
#ifdef DEBUG_OUTPUT
		if (nfds != nfds_old) {
			printf("\nnumber of fd: %d\n", nfds);
			nfds_old = nfds;
		}
#endif
		for (i = 0; i < nfds; ++i) {
			
			Package buffer;
			// if detect a new client connect to the server's socket, 
			// create a new connection
			if (event_array[i].data.fd == server_fd) {
				new_client_fd = accept(server_fd, (sockaddr * ) & client_addr, &length);
				
				if (new_client_fd < 0) {
#ifdef DEBUG_OUTPUT
					// output client fd
					cout << "Error, new_client_fd: " << new_client_fd << endl;
#endif
					perror("new_client_fd < 0");
					exit(1);
				}
				SetNonBlocking(new_client_fd);

#ifdef DEBUG_OUTPUT
				// output client fd
				char *str = inet_ntoa(client_addr.sin_addr);
				cout << "accept a connection from " << str;
				cout << "\tfd: " << new_client_fd << endl;
#endif
				
				//设置用于读操作的文件描述符
				event.data.fd = new_client_fd;
				
				//设置用于注测的读操作事件
#ifdef EDGE_TRIGGERED
				event.events = EPOLLIN | EPOLLET;
#else
				event.events = EPOLLIN;
#endif
				
				// register this client fd as neither receiving nor sending
				FDProcessStatus[(new_client_fd)] = ProcessStatus();
				// create the space 
				ClientInfoMap[new_client_fd] = Package(-1);
				
				// epoll_ctl - control interface for an epoll file descriptor
				// int epoll_ctl(int epoll_fd, int op, int fd, struct epoll_event *event);
				// It requests that the operation op be performed for the target file descriptor, fd.
				// add event
				epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_client_fd, &event);
			} else {
				
				if ((event_array[i].events & EPOLLIN)) {
					if (FDProcessStatus[event_array[i].data.fd].ready_to_receive_) {
						// go to receiving stage, so no longer in ready stage
						FDProcessStatus[event_array[i].data.fd].ready_to_receive_ = false;
#ifdef DEBUG_OUTPUT
						printf("fd: %d \t ready_to_receive_ polling\n", event_array[i].data.fd);
#endif
						Polling(event_array[i]);
					}
#ifdef DEBUG_OUTPUT
					printf("fd: %d \t waiting for ready_to_receive_ condition \n", event_array[i].data.fd);
#endif
					continue;
				} else {
					if ((event_array[i].events & EPOLLOUT)) {
						if (FDProcessStatus[event_array[i].data.fd].ready_to_send_) {
							// go to sending stage, so no longer in ready stage
							FDProcessStatus[event_array[i].data.fd].ready_to_send_ = false;
#ifdef DEBUG_OUTPUT
							printf("fd: %d \t ready_to_send_ polling\n", event_array[i].data.fd);
#endif
							Polling(event_array[i]);
						}
#ifdef DEBUG_OUTPUT
						printf("fd: %d \t waiting for ready_to_send_ condition \n", event_array[i].data.fd);
#endif
						continue;
					}
					perror("ERROR, events wrong type\n");
				}
				continue;
			}
		}
	}
	atexit(ProgramTerminated);
	return 0;
}