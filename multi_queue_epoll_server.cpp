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
#include "mullti_thread_worker.h"

using namespace std;
#define NUM_WORKERS            32
#define ECHO_SERVER_PORT        6000
#define NUM_FD                  1200
#define LISTEN_BACKLOG          16
#define MAX_EPOLL_EVENT_COUNT   (NUM_FD>>2)
#define EPOLL_WAIT_TIMEOUT      0


static std::vector<MultiThreadWorker> works(NUM_WORKERS);
static int epoll_fd, server_fd;
static volatile bool running = true;
// event for the server
static struct epoll_event event;

static unsigned long long JobCount = 0;
static unordered_map<int, Package> ClientInfoMap;
static unordered_map<int, ProcessStatus> FDProcessStatus;

// static Package ClientInfoArr[NUM_FD];
// static ProcessStatus FDProcessStatusArr[NUM_FD];
// todo may optimize unordered_map to array to accelerate
// static ProcessStatus FDProcessStatusArr[NUM_FD];


void process(struct epoll_event *epollEvent);

void ProgramTerminated();

void ReceivePackage(struct epoll_event *epollEvent);

void SendResult(struct epoll_event *epollEvent);

void sig_handler(int sig) {
	if (sig == SIGINT) {
		printf("JobCount:%llu\t\n", JobCount);
		ProgramTerminated();
	}
}

// back end thread
void ProcessJob(int i) {
	while (true) {
		struct epoll_event epollEvent = works[i].PopJob();
		
		if ((epollEvent.events) & EPOLLIN) {
			// receive from the client
			ReceivePackage(&epollEvent);
			// works[i].popJob();
		} else {
			if ((epollEvent.events) & EPOLLOUT) {
				// send to the client
				SendResult(&epollEvent);
				// works[i].popJob();
			} else {
				assert(false);
			}
		}
	}
}

// deprecated version
Package receivePackage(int client_fd) {
	// receive the Package from the client
	char recvPackage[PACKAGE_BUFFER_SIZE];
	Package buffer;
	memset(&buffer, 0, sizeof(Package)); // clean to 0
	ssize_t rs = recv(client_fd, recvPackage, PACKAGE_BUFFER_SIZE, 0);
	memcpy(&buffer, recvPackage, sizeof(Package));
#ifdef DEBUG_OUTPUT
	printf("recv over id:%u, key:%d, value:%d\n", (unsigned int) buffer.id, buffer.key.id, buffer.value.id);
#endif
	return buffer;
}

// deprecated SendResult
void sendResult(int client_fd, Package &buffer) {
	// send the response
	int sendbytes;
	Result result(buffer.id);
	if ((sendbytes = send(client_fd, (char *) &result, sizeof(Result), 0)) == -1) {
		perror("respond");
		exit(1);
	}
#ifdef DEBUG_OUTPUT
	printf("send the Result, id:%d\n", result.id);
#endif
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
	printf("start %s \n", argv[0]);
	
	signal(SIGINT, sig_handler);
	ClientInfoMap.reserve(NUM_FD);
	FDProcessStatus.reserve(NUM_FD);
	
	int i, maxi, server_fd, new_client_fd, sockfd, nfds/*number of fd s*/, portnumber;
	ssize_t n;
	socklen_t length;
	
	if (argc == 2) {
		if ((portnumber = atoi(argv[1])) < 0) {
			fprintf(stderr, "Usage:%s PORT_NUMBER/a/n", argv[0]);
			return 1;
		}
	} else {
		portnumber = SERVER_PORT;
	}
	
	//声明epoll_event结构体的变量,ev用于注册事件,数组用于回传要处理的事件
	
	struct epoll_event event_array[MAX_EPOLL_EVENT_COUNT];
	//生成用于处理accept的epoll专用的文件描述符
	
	epoll_fd = epoll_create(NUM_FD);
	struct sockaddr_in client_addr;// todo
	struct sockaddr_in serveraddr;
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	
	//把socket设置为非阻塞方式
	SetNonBlocking(server_fd);
	
	//设置与要处理的事件相关的文件描述符
	event.data.fd = server_fd;
	//设置要处理的事件类型
	
	// todo EPOLLET : 开启边缘触发，默认的是水平触发
	// we can adjust that 所以我们并未看到EPOLLLT
#ifdef EDGE_TRIGGERED
	event.events = EPOLLIN | EPOLLET;
#else
	event.events = EPOLLIN;
#endif
	
	//注册epoll事件
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
	// serveraddr.sin_addr.s_addr = htons(INADDR_ANY);
	string local_addr = "127.0.0.1";
	inet_aton(local_addr.c_str(), &(serveraddr.sin_addr));
	
	serveraddr.sin_port = htons(portnumber);
	bind(server_fd, (sockaddr * ) & serveraddr, sizeof(serveraddr));
	listen(server_fd, MAX_EPOLL_EVENT_COUNT);
	
	vector<Package> packages(MAX_EPOLL_EVENT_COUNT);
	
	// initializeWorkers thread to work
	std::vector<std::thread> workers(NUM_WORKERS);
	InitializeWorkers(workers);
	
	int nfds_old = 0;
	
	while (running) {
		//等待epoll事件的发生
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
			
			// todo debug code
			if (JobCount == 20001000LL) {
				printf("reach the JobCount\n");
				exit(0);
			}
			
			Package buffer;
			//如果新监测到一个SOCKET用户连接到了绑定的SOCKET端口，建立新的连接。
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
				
				// todo may optimize to increase speed
				// register this client fd as neither receiving nor sending
				FDProcessStatus[(new_client_fd)] = ProcessStatus();
				// create the space 
				ClientInfoMap[new_client_fd] = Package(-1);
				
				// epoll_ctl - control interface for an epoll file descriptor
				//  int epoll_ctl(int epoll_fd, int op, int fd, struct epoll_event *event);
				// It requests that the operation op be performed for the target file descriptor, fd.
				// add event
				epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_client_fd, &event);
			} else {
#ifdef PARALLEL_SEND_AND_RECEIVE
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
				printf("should not RUNNING_FLAG here\n");
#endif

#ifndef PARALLEL_SEND_AND_RECEIVE
#ifndef SEND_RECEIVE_SEPARATE
				// receive and send together
				
				// receive the Package from the client
				char recvPackage[PACKAGE_BUFFER_SIZE];
				// Package buffer;
				memset(&buffer, 0, sizeof(Package)); // clean to 0
				int ret = recv(event_array[i].data.fd, recvPackage, PACKAGE_BUFFER_SIZE, 0);
				if (ret <= 0) {
#ifdef DEBUG_OUTPUT
					printf("close connection from fd: %d \n", event_array[i].data.fd);
#endif
					close(event_array[i].data.fd);
					epoll_ctl(epoll_fd, EPOLL_CTL_DEL, event_array[i].data.fd, &event);
					continue;
				}
				memcpy(&buffer, recvPackage, sizeof(Package));
				packages[i] = (buffer);// may use std::move
#ifdef DEBUG_OUTPUT
				printf("recv over id:%u, key:%d, value:%d\n", (unsigned int) buffer.id, buffer.key.id, buffer.value.id);
#endif
				
				Result result(packages[i].id);
				int sendbytes = send(event_array[i].data.fd, (char *) &result, sizeof(Result), 0);
				if (sendbytes < 0) {
					perror("send failed.\n");
					continue;
				}
#ifdef DEBUG_OUTPUT
				printf("send the Result, id:%d\n", result.id);
#endif
#endif

#ifdef SEND_RECEIVE_SEPARATE
				
				//如果是已经连接的用户，并且收到数据，那么进行读入。
				if (event_array[i].events & EPOLLIN) {
#ifdef DEBUG_OUTPUT
					cout << "EPOLLIN" << endl;
#endif
					if ((sockfd = event_array[i].data.fd) < 0) {
#ifdef DEBUG_OUTPUT
						cout << "sockfd<0" << endl;
#endif
						continue;
					}
					// if ((n = read(sockfd, line, MAXLINE)) < 0) {
					// 	if (errno == ECONNRESET) {
					// 		close(sockfd);
					// 		event_array[i].data.fd = -1;
					// 	} else
					// 		std::cout << "readline error" << std::endl;
					// } else if (n == 0) {
					// 	close(sockfd);
					// 	event_array[i].data.fd = -1;
					// }
					
					// receive the Package from the client
					char recvPackage[PACKAGE_BUFFER_SIZE];
					// Package buffer;
					memset(&buffer, 0, sizeof(Package)); // clean to 0
					int ret = recv(event_array[i].data.fd, recvPackage, PACKAGE_BUFFER_SIZE, 0);
					if (ret <= 0) {
						close(event_array[i].data.fd);
						epoll_ctl(epoll_fd, EPOLL_CTL_DEL,
								  event_array[i].data.fd, &event);
						continue;
					}
					memcpy(&buffer, recvPackage, sizeof(Package));
					packages[i] = (buffer);// may use std::move
#ifdef DEBUG_OUTPUT
					printf("recv over id:%u, key:%d, value:%d\n", (unsigned int) buffer.id, buffer.key.id,
						   buffer.value.id);
#endif
					
					// int ret = recv(event_array[i].data.fd, buf, BUF_SIZE, 0);
					// if(ret <= 0) {
					// 	close(event_array[i].data.fd);
					// 	epoll_ctl(epoll_fd, EPOLL_CTL_DEL,
					// 	          event_array[i].data.fd, &event);
					// 	continue;
					// }
					
					//设置用于注测的写操作事件
					event.data.fd = sockfd;
					
					//修改sockfd上要处理的事件为EPOLLOUT
#ifdef EDGE_TRIGGERED
					event.events = EPOLLOUT | EPOLLET;
#else
					event.events = EPOLLOUT;
#endif
					
					epoll_ctl(epoll_fd, EPOLL_CTL_MOD, sockfd, &event);
					
				} else {
					// 如果有数据发送
					if (event_array[i].events & EPOLLOUT) {
						sockfd = event_array[i].data.fd;
						Result result(packages[i].id);
						int sendbytes = send(sockfd, (char *) &result, sizeof(Result), 0);
						if (sendbytes < 0) {
							perror("send failed.\n");
						}
#ifdef DEBUG_OUTPUT
						printf("send the Result, id:%d\n", result.id);
#endif
						//设置用于读操作的文件描述符
						// write(sockfd, line, n);
						
						//设置用于注测的读操作事件
						event.data.fd = sockfd;
						
						//修改sockfd上要处理的事件为EPOLIN
#ifdef EDGE_TRIGGERED
						event.events = EPOLLIN | EPOLLET;
#else
						event.events = EPOLLIN;
#endif
						
						epoll_ctl(epoll_fd, EPOLL_CTL_MOD, sockfd, &event);
						
					}
				}
#endif
#endif
			}
		}
		
		
	}
	atexit(ProgramTerminated);
	return 0;
}