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
#include "data.h"

static const int PORT_NUMBER = SERVER_PORT;
static const int NUM_FD = 1200;
static const int MAX_EVENT_COUNT = (NUM_FD >> 2);

void do_accept_cb(evutil_socket_t server_fd, short event, void *arg);

void read_cb(struct bufferevent *bufev, void *arg);

void error_cb(struct bufferevent *bufev, short event, void *arg);

void write_cb(struct bufferevent *bufev, void *arg);

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
	evutil_make_listen_socket_reuseable(server_fd);
	
	struct sockaddr_in server_addr;
	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	// server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	server_addr.sin_port = htons(PORT_NUMBER);
	
	if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
		perror("Error when bind");
		return 1;
	}
	
	if (listen(server_fd, MAX_EVENT_COUNT) < 0) {
		perror("Error when listen");
		return 1;
	}
	
	printf("Listening...\n");
	
	/** Do platform-specific operations as needed to make a socket nonblocking.
	    @param sock The socket to make nonblocking
	    @return 0 on success, -1 on failure
	*/
	evutil_make_socket_nonblocking(server_fd);
	
	/**
	 * Create and return a new event_base to use with the rest of Libevent.
	 * @return a new event_base on success, or NULL on failure.
	 * @see event_base_free(), event_base_new_with_config()
	 */
	struct event_base *base = event_base_new();
	if (!base) {
		fprintf(stderr, "Could not initialize libevent!\n");
		return 1;
	}
	
	// create and bind to an event
	struct event *listen_event;
	// event_new相当于epoll中的epoll_wait，其中的epoll里的while循环，在libevent里使用event_base_dispatch。
	// do_accept_cb：绑定的回调函数
	// (void*)base：给回调函数的参数
	listen_event = event_new(base, server_fd, EV_READ | EV_PERSIST, do_accept_cb, (void *) base);
	
	// event_add相当于epoll中的epoll_ctl，参数是EPOLL_CTL_ADD，添加事件。
	// 注册时间，参数NULL表示无超时设置
	event_add(listen_event, NULL);
	event_base_dispatch(base); // 程序进入无限循环，等待就绪事件并执行事件处理
	
	printf("The End. Should not exist\n");
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
	struct bufferevent *bufev = bufferevent_socket_new(base, new_client_fd, BEV_OPT_CLOSE_ON_FREE);
	
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
	}
	// todo how about n<=0, do we need to close?
	
	// todo original code
	// while (n = bufferevent_read(bufev, recv_package_buffer, sizeof(Package)), n > 0) {
	// 	printf("fd=%u, read line: %s\n", fd, recv_package_buffer);
	// 	// todo modify here
	// 	bufferevent_write(bufev, recv_package_buffer, n);
	// }
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