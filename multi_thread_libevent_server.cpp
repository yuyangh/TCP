#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <iostream>
#include <thread>
#include<netinet/in.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <unistd.h>
#include <vector>
#include <iostream>
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
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <iostream>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <csignal>
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/thread.h>
#include <event2/util.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include "event.h"
#include "data.h"
#include "ring_buffer.hpp"


typedef struct {
	// pthread_t tid;
	unsigned int id;
	struct event_base *base;
	struct event thread_event;
	int read_fd;
	int write_fd;
} LIBEVENT_THREAD;

typedef struct {
	// pthread_t tid;
	unsigned int id;
	struct event_base *base;
} DISPATCHER_THREAD;

static const int NUM_WORKERS = 20;
static const int PORT_NUMBER = SERVER_PORT;
static const int NUM_FD = 1200;
static const int MAX_EVENT_COUNT = (NUM_FD >> 2);
static const size_t MAX_QUEUE_SIZE = NUM_FD;
static unsigned long long JobCount = 0;

static bool OccupancyStatus[NUM_FD];
static RingBuffer<struct bufferevent *,true,true> BufferEventRingBuffer(MAX_QUEUE_SIZE);
static DISPATCHER_THREAD dispatcher_thread;
static LIBEVENT_THREAD threads[NUM_WORKERS];



void do_accept_cb(evutil_socket_t server_fd, short event, void *arg);

void read_cb(struct bufferevent *bufev, void *arg);

void error_cb(struct bufferevent *bufev, short event, void *arg);

void write_cb(struct bufferevent *bufev, void *arg);

void InitializeWorkers(std::vector<std::thread> &workers);

void ProcessJob(int i);

void Work(int i);

void InitializeOccupancyStatus();

// todo modify later
static void thread_libevent_process(int fd, short which, void *arg) {
	// int ret;
	// char buf[128];
	// LIBEVENT_THREAD *me = (LIBEVENT_THREAD *) arg;
	//
	// int socket_fd = recv_fd(me->read_fd);
	//
	// struct event *pReadEvent = NULL;
	// pReadEvent = (struct event *) malloc(sizeof(struct event));
	//
	// event_assign(pReadEvent, me->base, socket_fd, EV_READ | EV_PERSIST, Reader, NULL);
	//
	// event_add(pReadEvent, NULL);
	//
	// return;
}

void ListenAccept(int sock, short event, void *arg) {
	// printf("ListenAccept ................\n");
	// // 1,读 --也就是accept
	// struct sockaddr_in ClientAddr;
	// int nClientSocket = -1;
	// socklen_t ClientLen = sizeof(ClientAddr);
	// printf("---------------------------1\n");
	// nClientSocket = accept(sock, (struct sockaddr *) &ClientAddr, &ClientLen);
	// printf("---------------------------2,%d\n", nClientSocket);
	// if (-1 == nClientSocket) {
	// 	printf("accet error:%s\n", strerror(errno));
	// 	return;
	// }
	// fprintf(stderr, "a client connect to server ....\n");
	//
	// //进行数据分发
	// int id = (JobCount) % NUM_WORKERS;        //memcached中线程负载均衡算法
	// ++JobCount;
	//
	// LIBEVENT_THREAD *thread = threads[id];
	// send_fd(thread->write_fd, nClientSocket);
	//
	// return;
}

int main(int argc, char *argv[]) {
	
	printf("start %s \n", argv[0]);
	int ret;
	evutil_socket_t server_fd;
	server_fd = socket(AF_INET, SOCK_STREAM, 0);// fd being listened
	if (server_fd < 0) {
		printf("socket error!\n");
		return 1;
	}
	
	/** Do platform-specific operations to make a listener socket reusable.

	    Specifically, we want to make sure that another program will be able
	    to bind this address right after we've closed the listener.
	
	    This differs from Windows's interpretation of "reusable", which
	    allows multiple listeners to bind the same address at the same time.
	
	    @param sock The socket to make reusable
	    @return 0 on success, -1 on failure
 	*/
	// evutil_make_listen_socket_reuseable(server_fd);
	//
	// struct sockaddr_in server_addr;
	// bzero(&server_addr, sizeof(server_addr));
	// server_addr.sin_family = AF_INET;
	// server_addr.sin_addr.s_addr = htons(INADDR_ANY);
	// // server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	// server_addr.sin_port = htons(PORT_NUMBER);
	//
	// if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
	// 	perror("Error when bind");
	// 	return 1;
	// }
	//
	// if (listen(server_fd, MAX_EVENT_COUNT) < 0) {
	// 	perror("Error when listen");
	// 	return 1;
	// }
	//
	// printf("Listening...\n");
	//
	//
	// // int ret;
	// int i;
	// int fd[2];
	//
	// DispatcherThread.base = event_init();
	// if (DispatcherThread.base == NULL) {
	// 	perror("event_init( base )");
	// 	return 1;
	// }
	//
	// /** Do platform-specific operations as needed to make a socket nonblocking.
	//     @param sock The socket to make nonblocking
	//     @return 0 on success, -1 on failure
	// */
	// evutil_make_socket_nonblocking(server_fd);
	//
	//
	//
	// for (i = 0; i < NUM_WORKERS; i++) {
	// 	/* Create two new sockets, of type TYPE in domain DOMAIN and using
	// 	   protocol PROTOCOL, which are connected to each other, and put file
	// 	   descriptors for them in FDS[0] and FDS[1].  If PROTOCOL is zero,
	// 	   one will be chosen automatically.  Returns 0 on success, -1 for errors.  */
	// 	ret = socketpair(AF_LOCAL, SOCK_STREAM, 0, fd);
	//
	// 	if (ret == -1) {
	// 		perror("socketpair()");
	// 		return 1;
	// 	}
	//
	// 	threads[i].read_fd = fd[1];
	// 	threads[i].write_fd = fd[0];
	//
	// 	threads[i].base = event_init();
	// 	if (threads[i].base == NULL) {
	// 		perror("event_init()");
	// 		return 1;
	// 	}
	//
	// 	//工作线程处理可读处理
	// 	&threads[i].thread_event = event_new(threads[i].base, threads[i].read_fd,
	// 			EV_READ | EV_PERSIST, thread_libevent_process, &threads[i]);
	// 	// deprecated version
	// 	// event_set(&threads[i].event, threads[i].read_fd, EV_READ | EV_PERSIST, thread_libevent_process, &threads[i]);
	// 	// /**
	// 	//   Associate a different event base with an event.
	// 	//   The event to be associated must not be currently active or pending.
	// 	//
	// 	//   @param eb the event base
	// 	//   @param ev the event
	// 	//   @return 0 on success, -1 on failure.
	// 	//  */
	// 	// event_base_set(threads[i].base, &threads[i].event);
	//
	// 	// Add an event to the set of pending events.
	// 	if (event_add(&threads[i].thread_event, 0) == -1) {
	// 		perror("event_add()");
	// 		return 1;
	// 	}
	// }
	//
	// // initialize Workers thread to work
	// std::vector<std::thread> workers(NUM_WORKERS);
	//
	// // do initialization
	// InitializeWorkers(workers);
	//
	// struct event ListenEvent;
	//
	// //3, 把事件，套接字，libevent的管理器给管理起来， 也叫注册
	// //int event_assign(struct event *, struct event_base *, evutil_socket_t, short, event_callback_fn, void *);
	// if (-1 == event_assign(&ListenEvent, DispatcherThread.base, server_fd,
	//                        EV_READ | EV_PERSIST, ListenAccept, NULL)) {
	// 	printf("event_assign error:%s\n", strerror(errno));
	// 	return -1;
	// }
	//
	// // 4, 让我们注册的事件 可以被调度
	// if (-1 == event_add(&ListenEvent, NULL)) {
	// 	printf("event_add error:%s\n", strerror(errno));
	// 	return -1;
	// }
	//
	// printf("libvent start run ...\n");
	// // 5,运行libevent
	// if (-1 == event_base_dispatch(DispatcherThread.base)) {
	// 	printf("event_base_dispatch error:%s\n", strerror(errno));
	// 	return -1;
	// }
	//
	//
	// ////////////////////////////////////////////////////////////////
	//
	//
	//
	// /** Do platform-specific operations as needed to make a socket nonblocking.
	//     @param sock The socket to make nonblocking
	//     @return 0 on success, -1 on failure
	// */
	// evutil_make_socket_nonblocking(server_fd);
	//
	// /**
	//  * Create and return a new event_base to use with the rest of Libevent.
	//  * @return a new event_base on success, or NULL on failure.
	//  * @see event_base_free(), event_base_new_with_config()
	//  */
	// struct event_base *base = event_base_new();
	// if (!base) {
	// 	fprintf(stderr, "Could not initialize libevent!\n");
	// 	return 1;
	// }
	// evthread_use_pthreads();
	//
	// // initialize Workers thread to work
	// std::vector<std::thread> workers(NUM_WORKERS);
	//
	// // do initialization
	// InitializeWorkers(workers);
	// InitializeOccupancyStatus();
	//
	// // create and bind to an event
	// struct event *listen_event;
	// // event_new相当于epoll中的epoll_wait，其中的epoll里的while循环，在libevent里使用event_base_dispatch。
	// // do_accept_cb：绑定的回调函数
	// // (void*)base：给回调函数的参数
	// listen_event = event_new(base, server_fd, EV_READ | EV_PERSIST, do_accept_cb, (void *) base);
	//
	// // event_add相当于epoll中的epoll_ctl，参数是EPOLL_CTL_ADD，添加事件。
	// // 注册时间，参数NULL表示无超时设置
	// event_add(listen_event, NULL);
	// event_base_dispatch(base); // 程序进入无限循环，等待就绪事件并执行事件处理
	//
	// event_free(listen_event);
	// event_base_free(base);
	//
	// printf("The End. Should not exist\n");
	return 0;
}

void do_accept_cb(evutil_socket_t server_fd, short event, void *arg) {
	struct event_base *base = (struct event_base *) arg;
	evutil_socket_t new_client_fd;
	struct sockaddr_in client_addr;// todo may move to do optimization
	socklen_t length;
	new_client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &length);
	if (new_client_fd < 0) {
		perror("accept");
		return;
	}

#ifdef DEBUG_OUTPUT
	printf("accept: new_client_fd = %u\n", new_client_fd);
#endif
	
	//使用bufferevent_socket_new创建一个struct bufferevent *bufev，关联该sockfd，托管给event_base
	/** BEV_OPT_CLOSE_ON_FREE
	 * If set, we close the underlying file
	 * descriptor/bufferevent/whatever when this bufferevent is freed. */
	struct bufferevent *bufev = bufferevent_socket_new(base, new_client_fd,
			BEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE|LEV_OPT_THREADSAFE);
	
	/**
	  Changes the callbacks for a bufferevent.
	  @param bufev the bufferevent object for which to change callbacks
	  @param readcb callback to invoke when there is data to be read, or NULL if
		 no callback is desired
	  @param writecb callback to invoke when the file descriptor is ready for
		 writing, or NULL if no callback is desired
	  @param eventcb callback to invoke when there is an event on the file
		 descriptor
	  @param cbarg an argument that will be supplied to each of the callbacks
		 (readcb, writecb, and errorcb)
	  @see bufferevent_new()
  	*/
	bufferevent_setcb(bufev, read_cb, NULL, error_cb, arg);
	
	/**
	  Enable a bufferevent.
	  @param bufev the bufferevent to be enabled
	  @param event any combination of EV_READ | EV_WRITE.
	  @return 0 if successful, or -1 if an error occurred
	  @see bufferevent_disable()
 	*/
	/**
	* Persistent event: won't get removed automatically when activated.
	* When a persistent event with a timeout becomes activated, its timeout
	* is reset to 0.
	*/
	bufferevent_enable(bufev, EV_READ | EV_WRITE | EV_PERSIST);
}

void read_cb(struct bufferevent *bufev, void *arg) {
	if(OccupancyStatus[bufferevent_getfd(bufev)]==false){
		OccupancyStatus[bufferevent_getfd(bufev)]=true;
#ifdef DEBUG_OUTPUT
		printf("add bufferevent fd:\t%d \n",bufferevent_getfd(bufev));
#endif
		BufferEventRingBuffer.Push(bufev);
	}
// 	char recv_package_buffer[PACKAGE_BUFFER_SIZE];
// 	int n;
// 	/**
// 	   Returns the file descriptor associated with a bufferevent, or -1 if
// 	   no file descriptor is associated with the bufferevent.
// 	 */
// 	// evutil_socket_t is an int
// 	evutil_socket_t fd = bufferevent_getfd(bufev);
//
// 	Package package_buffer;
// 	memset(&package_buffer, 0, sizeof(Package)); // clean to 0
// 	/**
// 	  Read data from a bufferevent buffer.
// 	  The bufferevent_read() function is used to read data from the input buffer.
//
// 	  @param bufev the bufferevent to be read from
// 	  @param data pointer to a buffer that will store the data
// 	  @param size the size of the data buffer, in bytes
// 	  @return the amount of data read, in bytes.
// 	 */
// 	n = bufferevent_read(bufev, recv_package_buffer, sizeof(Package));
// 	// todo debug output
// #ifdef DEBUG_OUTPUT
// 	printf("size read:\t%d, sizeof(Package):\t%d \n",n,(int)sizeof(Package));
// #endif
// 	if (n > 0) {
// 		memcpy(&package_buffer, recv_package_buffer, sizeof(Package));
// 		Result result_buffer(package_buffer.value.id);
// 		/**
// 		  Write data to a bufferevent buffer.
// 		  The bufferevent_write() function can be used to write data to the file
// 		  descriptor.  The data is appended to the output buffer and written to the
// 		  descriptor automatically as it becomes available for writing.
//
// 		  @param bufev the bufferevent to be written to
// 		  @param data a pointer to the data to be written
// 		  @param size the length of the data, in bytes
// 		  @return 0 if successful, or -1 if an error occurred
// 		  @see bufferevent_write_buffer()
// 		  */
// 		bufferevent_write(bufev, &result_buffer, sizeof(Result));
// 	}
	// todo how about n<=0, do we need to close?
}

void write_cb(struct bufferevent *bufev, void *arg) {}

void error_cb(struct bufferevent *bufev, short event, void *arg) {
	evutil_socket_t fd = bufferevent_getfd(bufev);
#ifdef DEBUG_OUTPUT
	printf("fd = %u, ", fd);
	if (event & BEV_EVENT_TIMEOUT) {
		printf("Timed out\n"); //if bufferevent_set_timeouts() called
	} else if (event & BEV_EVENT_EOF) {
		printf("connection closed\n");
	} else if (event & BEV_EVENT_ERROR) {
		printf("some other error\n");
	}
#endif
	bufferevent_free(bufev);
}

void InitializeWorkers(std::vector<std::thread> &workers) {
	for (int i = 0; i < NUM_WORKERS; ++i) {
		workers.push_back(std::thread(Work, i));
		if (workers[i].joinable()) {
			workers[i].detach();
		}
	}
}

// back end thread
void Work(int i) {
	while (true) {
		struct bufferevent *bufev = BufferEventRingBuffer.Pop();
#ifdef DEBUG_OUTPUT
		printf("process bufferevent fd:\t%d \n",bufferevent_getfd(bufev));
#endif
		char recv_package_buffer[PACKAGE_BUFFER_SIZE];
		int n;
		/**
	   Returns the file descriptor associated with a bufferevent, or -1 if
	   no file descriptor is associated with the bufferevent.
	 */
		// evutil_socket_t is an int
		evutil_socket_t fd = bufferevent_getfd(bufev);
		
		Package package_buffer;
		memset(&package_buffer, 0, sizeof(Package)); // clean to 0
		/**
		  Read data from a bufferevent buffer.
		  The bufferevent_read() function is used to read data from the input buffer.
		
		  @param bufev the bufferevent to be read from
		  @param data pointer to a buffer that will store the data
		  @param size the size of the data buffer, in bytes
		  @return the amount of data read, in bytes.
		 */
		n = bufferevent_read(bufev, recv_package_buffer, sizeof(Package));
		// todo debug output
#ifdef DEBUG_OUTPUT
		printf("size read:\t%d, sizeof(Package):\t%d \n",n,(int)sizeof(Package));
#endif
		if (n > 0) {
			memcpy(&package_buffer, recv_package_buffer, sizeof(Package));
			Result result_buffer(package_buffer.value.id);
			/**
			  Write data to a bufferevent buffer.
			  The bufferevent_write() function can be used to write data to the file
			  descriptor.  The data is appended to the output buffer and written to the
			  descriptor automatically as it becomes available for writing.
			
			  @param bufev the bufferevent to be written to
			  @param data a pointer to the data to be written
			  @param size the length of the data, in bytes
			  @return 0 if successful, or -1 if an error occurred
			  @see bufferevent_write_buffer()
			  */
			bufferevent_write(bufev, &result_buffer, sizeof(Result));
			OccupancyStatus[bufferevent_getfd(bufev)]=false;
		}
		// todo how about n<=0, do we need to close?
	}
}


// back end thread
void ProcessJob(int i) {
	while (true) {
		struct bufferevent *bufev = BufferEventRingBuffer.Pop();
#ifdef DEBUG_OUTPUT
		printf("process bufferevent fd:\t%d \n",bufferevent_getfd(bufev));
#endif
		char recv_package_buffer[PACKAGE_BUFFER_SIZE];
		int n;
		/**
	   Returns the file descriptor associated with a bufferevent, or -1 if
	   no file descriptor is associated with the bufferevent.
	 */
		// evutil_socket_t is an int
		evutil_socket_t fd = bufferevent_getfd(bufev);
		
		Package package_buffer;
		memset(&package_buffer, 0, sizeof(Package)); // clean to 0
		/**
		  Read data from a bufferevent buffer.
		  The bufferevent_read() function is used to read data from the input buffer.
		
		  @param bufev the bufferevent to be read from
		  @param data pointer to a buffer that will store the data
		  @param size the size of the data buffer, in bytes
		  @return the amount of data read, in bytes.
		 */
		n = bufferevent_read(bufev, recv_package_buffer, sizeof(Package));
		// todo debug output
#ifdef DEBUG_OUTPUT
		printf("size read:\t%d, sizeof(Package):\t%d \n",n,(int)sizeof(Package));
#endif
		if (n > 0) {
			memcpy(&package_buffer, recv_package_buffer, sizeof(Package));
			Result result_buffer(package_buffer.value.id);
			/**
			  Write data to a bufferevent buffer.
			  The bufferevent_write() function can be used to write data to the file
			  descriptor.  The data is appended to the output buffer and written to the
			  descriptor automatically as it becomes available for writing.
			
			  @param bufev the bufferevent to be written to
			  @param data a pointer to the data to be written
			  @param size the length of the data, in bytes
			  @return 0 if successful, or -1 if an error occurred
			  @see bufferevent_write_buffer()
			  */
			bufferevent_write(bufev, &result_buffer, sizeof(Result));
			OccupancyStatus[bufferevent_getfd(bufev)]=false;
		}
		// todo how about n<=0, do we need to close?
	}
}

void InitializeOccupancyStatus() {
	for (auto &item : OccupancyStatus) {
		item = false;
	}
}