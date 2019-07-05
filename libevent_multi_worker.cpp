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
#include <fcntl.h>

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
	
	return recv_fd;
}

//-------------------------------------------------
typedef struct {
	pthread_t tid;
	struct event_base *base;
	struct event event;
	int read_fd;
	int write_fd;
} LIBEVENT_THREAD;

typedef struct {
	pthread_t tid;
	struct event_base *base;
} DISPATCHER_THREAD;


const int thread_num = 10;

LIBEVENT_THREAD *threads;
DISPATCHER_THREAD dispatcher_thread;
int last_thread = 0;
//-------------------------------------------------

unsigned short nPort = 9000;
struct event_base *pEventMgr = NULL;

void Reader(int sock, short event, void *arg) {
	fprintf(stderr, "reader ---------------%d\n", sock);
	char buffer[1024];
	memset(buffer, 0, sizeof(buffer));
	
	int ret = -1;
	fprintf(stderr, "1 ---------------%d\n", sock);
	ret = recv(sock, buffer, sizeof(buffer), 0);
	fprintf(stderr, "2 ---------------%d\n", sock);
	if (-1 == ret || 0 == ret) {
		printf("recv error:%s\n", strerror(errno));
		return;
	}
	
	printf("recv data:%s\n", buffer);
	
	
	//strcat(buffer,", hello,client\n");
	//send(sock,buffer,strlen(buffer),0);
	
	
	return;
}

static void thread_libevent_process(int fd, short which, void *arg) {
	int ret;
	char buf[128];
	LIBEVENT_THREAD *me = (LIBEVENT_THREAD *) arg;
	
	int socket_fd = recv_fd(me->read_fd);
	
	struct event *pReadEvent = NULL;
	pReadEvent = (struct event *) malloc(sizeof(struct event));
	
	event_assign(pReadEvent, me->base, socket_fd, EV_READ | EV_PERSIST, Reader, NULL);
	
	event_add(pReadEvent, NULL);
	
	return;
}

static void *worker_thread(void *arg) {
	
	LIBEVENT_THREAD *me = (LIBEVENT_THREAD *) arg;
	me->tid = pthread_self();
	
	event_base_loop(me->base, 0);
	
	
	return NULL;
}


void ListenAccept(int sock, short event, void *arg) {
	printf("ListenAccept ................\n");
	// 1,读 --也就是accept
	struct sockaddr_in ClientAddr;
	int nClientSocket = -1;
	socklen_t ClientLen = sizeof(ClientAddr);
	printf("---------------------------1\n");
	nClientSocket = accept(sock, (struct sockaddr *) &ClientAddr, &ClientLen);
	printf("---------------------------2,%d\n", nClientSocket);
	if (-1 == nClientSocket) {
		printf("accet error:%s\n", strerror(errno));
		return;
	}
	fprintf(stderr, "a client connect to server ....\n");
	
	//进行数据分发
	int tid = (last_thread + 1) % thread_num;        //memcached中线程负载均衡算法
	LIBEVENT_THREAD *thread = threads + tid;
	last_thread = tid;
	send_fd(thread->write_fd, nClientSocket);
	
	return;
}


int main() {
	
	int nSocket = -1;
	int nRet = -1;
	
	nSocket = socket(PF_INET, SOCK_STREAM, 0);
	if (-1 == nSocket) //
	{
		printf("socket error:%s\n", strerror(errno));
		return -1;
	}
	
	int value = 1;
	setsockopt(nSocket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));
	
	
	struct sockaddr_in ServerAddr;
	ServerAddr.sin_family = PF_INET;
	ServerAddr.sin_port = htons(nPort);
	ServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	nRet = bind(nSocket, (struct sockaddr *) &ServerAddr, (socklen_t)
	sizeof(ServerAddr));
	if (-1 == nRet) {
		printf("bind error:%s\n", strerror(errno));
		return -1;
	}
	
	
	//int listen(int sockfd, int backlog);
	nRet = listen(nSocket, 100);
	if (-1 == nRet) {
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
	
	dispatcher_thread.base = event_init();
	if (dispatcher_thread.base == NULL) {
		perror("event_init( base )");
		return 1;
	}
	dispatcher_thread.tid = pthread_self();
	
	threads = (LIBEVENT_THREAD *) calloc(thread_num, sizeof(LIBEVENT_THREAD));
	if (threads == NULL) {
		perror("calloc");
		return 1;
	}
	
	for (i = 0; i < thread_num; i++) {
		/* Create two new sockets, of type TYPE in domain DOMAIN and using
		   protocol PROTOCOL, which are connected to each other, and put file
		   descriptors for them in FDS[0] and FDS[1].  If PROTOCOL is zero,
		   one will be chosen automatically.  Returns 0 on success, -1 for errors.  */
		ret = socketpair(AF_LOCAL, SOCK_STREAM, 0, fd);
		
		if (ret == -1) {
			perror("socketpair()");
			return 1;
		}
		
		threads[i].read_fd = fd[1];
		threads[i].write_fd = fd[0];
		
		threads[i].base = event_init();
		if (threads[i].base == NULL) {
			perror("event_init()");
			return 1;
		}
		
		//工作线程处理可读处理
		event_set(&threads[i].event, threads[i].read_fd, EV_READ | EV_PERSIST, thread_libevent_process, &threads[i]);
		event_base_set(threads[i].base, &threads[i].event);
		if (event_add(&threads[i].event, 0) == -1) {
			perror("event_add()");
			return 1;
		}
	}
	
	for (i = 0; i < thread_num; i++) {
		pthread_create(&tid, NULL, worker_thread, &threads[i]);
	}


//2,创建具体的事件,
	
	struct event ListenEvent;
	
	//3, 把事件，套接字，libevent的管理器给管理起来， 也叫注册
	//int event_assign(struct event *, struct event_base *, evutil_socket_t, short, event_callback_fn, void *);
	if (-1 == event_assign(&ListenEvent, dispatcher_thread.base, nSocket, EV_READ | EV_PERSIST, ListenAccept, NULL)) {
		printf("event_assign error:%s\n", strerror(errno));
		return -1;
	}
	
	// 4, 让我们注册的事件 可以被调度
	if (-1 == event_add(&ListenEvent, NULL)) {
		printf("event_add error:%s\n", strerror(errno));
		return -1;
	}
	
	printf("libvent start run ...\n");
	// 5,运行libevent
	if (-1 == event_base_dispatch(dispatcher_thread.base)) {
		printf("event_base_dispatch error:%s\n", strerror(errno));
		return -1;
	}
	
	printf("---------------------------\n");


//--------------------------------------------------------------------------------------
	
	
	return 0;
}