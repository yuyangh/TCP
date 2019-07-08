/***********************************************************************************************************
,编译程序 
g++ -Wall -g  event-server.c -o server -levent -lpthread
*************************************************************************************************************/
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <errno.h>
#include "event.h"
#include <stdlib.h>
#include <pthread.h>
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/thread.h>
#include <event2/util.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <fcntl.h>
#include "data.h"
#include "ring_buffer.hpp"

#define ERR_EXIT(m) \
        do\
        { \
                perror(m); \
                exit(EXIT_FAILURE); \
        } while(0)\


void send_fd(int sock_fd, int send_fd) {
	int ret;
	struct msghdr msg;
	struct cmsghdr *p_cmsg;
	struct iovec vec;
	char cmsgbuf[CMSG_SPACE(sizeof(send_fd))];
	int *p_fds;
	char sendchar = 0;
	msg.msg_control = cmsgbuf;
	msg.msg_controllen = sizeof(cmsgbuf);
	p_cmsg = CMSG_FIRSTHDR(&msg);
	p_cmsg->cmsg_level = SOL_SOCKET;
	p_cmsg->cmsg_type = SCM_RIGHTS;
	p_cmsg->cmsg_len = CMSG_LEN(sizeof(send_fd));
	p_fds = (int *) CMSG_DATA(p_cmsg);
	*p_fds = send_fd; // 通过传递辅助数据的方式传递文件描述符
	
	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = &vec;
	msg.msg_iovlen = 1; //主要目的不是传递数据，故只传1个字符
	msg.msg_flags = 0;
	
	vec.iov_base = &sendchar;
	vec.iov_len = sizeof(sendchar);
	ret = sendmsg(sock_fd, &msg, 0);
	if (ret != 1)
		ERR_EXIT("sendmsg");
#ifdef DEBUG_OUTPUT
	printf("sock_fd:\t%d\n", sock_fd);
	printf("send_fd:\t%d\n", send_fd);
	// printf("sock_fd:\t%d\n", sock_fd);
#endif
// todo need this
	close(send_fd);
}

int recv_fd(const int sock_fd) {
	int ret;
	struct msghdr msg;
	char recvchar;
	struct iovec vec;
	int recv_fd;
	char cmsgbuf[CMSG_SPACE(sizeof(recv_fd))];
	struct cmsghdr *p_cmsg;
	int *p_fd;
	vec.iov_base = &recvchar;
	vec.iov_len = sizeof(recvchar);
	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = &vec;
	msg.msg_iovlen = 1;
	msg.msg_control = cmsgbuf;
	msg.msg_controllen = sizeof(cmsgbuf);
	msg.msg_flags = 0;
	
	p_fd = (int *) CMSG_DATA(CMSG_FIRSTHDR(&msg));
	*p_fd = -1;
	ret = recvmsg(sock_fd, &msg, 0);
	if (ret != 1)
		ERR_EXIT("recvmsg");
	
	p_cmsg = CMSG_FIRSTHDR(&msg);
	if (p_cmsg == NULL)
		ERR_EXIT("no passed fd");
	
	
	p_fd = (int *) CMSG_DATA(p_cmsg);
	recv_fd = *p_fd;
	if (recv_fd == -1)
		ERR_EXIT("no passed fd");
#ifdef DEBUG_OUTPUT
	printf("sock_fd:\t%d\n", sock_fd);
	printf("recv_fd:\t%d\n", recv_fd);
	// printf("sock_fd:\t%d\n", sock_fd);
#endif
	return recv_fd;
}

//-------------------------------------------------
typedef struct {
	pthread_t tid;
	struct event_base *base;
	struct event *thread_event;
	int read_fd;
	int write_fd;
} LIBEVENT_THREAD;

typedef struct {
	pthread_t tid;
	struct event_base *base;
} DISPATCHER_THREAD;

//-------------------------------------------------
const int NUM_WORKERS = 20;
static const int NUM_FD = 1200;
static const int MAX_EVENT_COUNT = (NUM_FD >> 2);

static LIBEVENT_THREAD threads[NUM_WORKERS];
static DISPATCHER_THREAD dispatcher_thread;
int last_thread = 0;
//-------------------------------------------------

static const unsigned short PORT_NUMBER = SERVER_PORT;
struct event_base *pEventMgr = NULL;

void Reader(int sock, short event, void *arg) {
	// fprintf(stderr, "reader ---------------%d\n", sock);
	// char buffer[1024];
	// memset(buffer, 0, sizeof(buffer));
	//
	// int ret = -1;
	//
	// fprintf(stderr, "1 ---------------%d\n", sock);
	// ret = recv(sock, buffer, sizeof(buffer), 0);
	// fprintf(stderr, "2 ---------------%d\n", sock);
	// if (-1 == ret || 0 == ret) {
	// 	printf("recv error:%s\n", strerror(errno));
	// 	return;
	// }
	//
	// printf("recv data:%s\n", buffer);
	
	
	Package package_buffer;
	char recv_package_buffer[PACKAGE_BUFFER_SIZE];
	// Package package_buffer;
	memset(&package_buffer, 0, sizeof(Package)); // clean to 0
	int ret = recv(sock, recv_package_buffer, PACKAGE_BUFFER_SIZE, 0);
	if (ret <= 0) {
#ifdef DEBUG_OUTPUT
		printf("close connection from fd: %d \n", sock);
#endif
		// todo may need optimize
		// evutil_closesocket(sock);
		close(sock);
		event_del((struct event *) arg);
		event_free((struct event *) arg);
		return;
	} else {
		memcpy(&package_buffer, recv_package_buffer, sizeof(Package));
#ifdef DEBUG_OUTPUT
		printf("fd: %d \t recv over id:%u, key:%d, value:%d\n", sock, (unsigned int) package_buffer.id,
			   package_buffer.key.id, package_buffer.value.id);
#endif
	}
	
	///////////////////////////////////////////////////////
	
	// send the response
	
	// Result result(ClientInfoArr[epollEventFD].value.id);
	Result result(package_buffer.value.id);
	int sendbytes = send(sock, (char *) &result, sizeof(Result), 0);
	if (sendbytes < 0) {
		perror("send failed.\n");
		return;
	}
#ifdef DEBUG_OUTPUT
	printf("fd: %d \t send the Result, id:%d\n", sock, result.id);
#endif

}

static void thread_libevent_process(int fd, short which, void *arg) {
	LIBEVENT_THREAD *current_thread = (LIBEVENT_THREAD *) arg;
	
	int socket_fd = recv_fd(current_thread->read_fd);
#ifdef DEBUG_OUTPUT
	printf("current_thread->read_fd:\t%d\n", current_thread->read_fd);
	printf("current_thread->write_fd:\t%d\n", current_thread->write_fd);
	printf("socket_fd:\t%d\n", socket_fd);
#endif
	struct event *thread_read_event = nullptr;
	// cannot use event_new because we need to bind the argument
	thread_read_event = (struct event *) malloc(sizeof(struct event));
	event_assign(thread_read_event, current_thread->base, socket_fd, EV_READ | EV_PERSIST,
	             Reader, thread_read_event);
	// thread_read_event=event_new(current_thread->base, socket_fd, EV_READ | EV_PERSIST,
	//           Reader, thread_read_event);
	
	event_add(thread_read_event, nullptr);
	
}

static void *worker_thread(void *arg) {
	LIBEVENT_THREAD *me = (LIBEVENT_THREAD *) arg;
	me->tid = pthread_self();
	// because at the beginning, there are no events,
	// so we need event_base_loop instead of event_base_dispatch()
	
	/**
	  Wait for events to become active, and run their callbacks.
	  This is a more flexible version of event_base_dispatch().
	
	  By default, this loop will run the event base until either there are no more
	  pending or active events, or until something calls event_base_loopbreak() or
	  event_base_loopexit().  You can override this behavior with the 'flags'
	  argument.
	
	  @param eb the event_base structure returned by event_base_new() or
	     event_base_new_with_config()
	  @param flags any combination of EVLOOP_ONCE | EVLOOP_NONBLOCK
	  @return 0 if successful, -1 if an error occurred, or 1 if we exited because
	     no events were pending or active.
	  */
	event_base_loop(me->base, 0);
	return nullptr;
}


void ListenAccept(int sock, short event, void *arg) {
#ifdef DEBUG_OUTPUT
	printf("ListenAccept ................\n");
#endif
	// 1,读 --也就是accept
	struct sockaddr_in ClientAddr;
	int nClientSocket = -1;
	socklen_t ClientLen = sizeof(ClientAddr);
	
	nClientSocket = accept(sock, (struct sockaddr *) &ClientAddr, &ClientLen);
#ifdef DEBUG_OUTPUT
	printf("---------------------------%d\n", nClientSocket);
#endif
	if (-1 == nClientSocket) {
		printf("accet error:%s\n", strerror(errno));
		return;
	}
#ifdef DEBUG_OUTPUT
	printf("accept a client connection from:\t%d\n", nClientSocket);
#endif
	
	//进行数据分发
	int tid = (last_thread + 1) % NUM_WORKERS;        //memcached中线程负载均衡算法
	LIBEVENT_THREAD *thread = threads + tid;
	last_thread = tid;
	send_fd(thread->write_fd, nClientSocket);
}


int main(int argc, char **argv) {
	printf("start %s \n", argv[0]);
	int server_fd = -1;
	int nRet = -1;
	
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (-1 == server_fd) //
	{
		printf("socket error:%s\n", strerror(errno));
		return -1;
	}
	
	int value = 1;
	// original version
	// setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));
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
		printf("bind error:%s\n", strerror(errno));
		return -1;
	}
	
	//int listen(int sockfd, int backlog);
	if (listen(server_fd, MAX_EVENT_COUNT) < 0) {
		printf("listen error:%s\n", strerror(errno));
		return -1;
	}
	
	printf("Listening...\n");


//开始创建libvent
//--主线程只管监听socket,连接socket由工作线程来管理------------------------------------
//当有新的连接到来时，主线程就接受之并将新返回的连接socket派发给某个工作线程
//此后该新socket上的任何I/O操作都有被选中的工作线程来处理
//工作线程检测到管道上有数据可读
//--------------------------------------------------------------------------------------
	int ret;
	int i;
	int fd[2];
	
	pthread_t tid;
	
	dispatcher_thread.base = event_base_new();
	if (dispatcher_thread.base == NULL) {
		perror("event_init( base )");
		return 1;
	}
	dispatcher_thread.tid = pthread_self();
	
	/** Do platform-specific operations as needed to make a socket nonblocking.
	//     @param sock The socket to make nonblocking
	//     @return 0 on success, -1 on failure
	// */
	evutil_make_socket_nonblocking(server_fd);
	
	for (i = 0; i < NUM_WORKERS; i++) {
		/* Create two new sockets, of type TYPE in domain DOMAIN and using
		   protocol PROTOCOL, which are connected to each other, and put file
		   descriptors for them in FDS[0] and FDS[1].  If PROTOCOL is zero,
		   one will be chosen automatically.  Returns 0 on success, -1 for errors.  */
		if (socketpair(AF_LOCAL, SOCK_STREAM, 0, fd) < 0) {
			perror("socketpair()");
			return 1;
		}
		
		threads[i].read_fd = fd[1];
		threads[i].write_fd = fd[0];
		
		threads[i].base = event_base_new();
		if (threads[i].base == nullptr) {
			perror("event_init()");
			return 1;
		}
		
		// worker thread make its event able to read
		// deprecated version
		// event_set(&threads[i].thread_event, threads[i].read_fd, EV_READ | EV_PERSIST,
		//           thread_libevent_process, &threads[i]);
		// event_base_set(threads[i].base, &threads[i].thread_event);
		threads[i].thread_event = event_new(threads[i].base, threads[i].read_fd,
		                                    EV_READ | EV_PERSIST, thread_libevent_process, &threads[i]);
		
		if (event_add(threads[i].thread_event, 0) == -1) {
			perror("event_add()");
			return 1;
		}
	}
	
	// create and start pthreads to work
	for (i = 0; i < NUM_WORKERS; i++) {
		pthread_create(&tid, NULL, worker_thread, &threads[i]);
	}
	
	//2,创建具体的事件,
	struct event *ListenEvent;
	
	//3, 把事件，套接字，libevent的管理器给管理起来， 也叫注册
	ListenEvent = event_new(dispatcher_thread.base, server_fd, EV_READ | EV_PERSIST, ListenAccept, nullptr);
	
	// 4, 让我们注册的事件 可以被调度
	if (-1 == event_add(ListenEvent, NULL)) {
		printf("event_add error:%s\n", strerror(errno));
		return -1;
	}
	
	// 5,运行libevent
	printf("libvent starts running ...\n");
	if (-1 == event_base_dispatch(dispatcher_thread.base)) {
		printf("event_base_dispatch error:%s\n", strerror(errno));
		return -1;
	}
	
	event_free(ListenEvent);
	printf("The End. Should not exist\n");
	return 0;
}