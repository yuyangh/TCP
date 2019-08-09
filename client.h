//
// Created by yuyanghu on 6/11/2019.
//

#ifndef TCPEXAMPLE_CLIENT_H
#define TCPEXAMPLE_CLIENT_H

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/shm.h>
#include <pthread.h>
#include <string>
#include <iostream>
#include "data.h"


class Client {
public:
	Client() : serverAddress("127.0.0.1"), socketServer(socket(AF_INET, SOCK_STREAM, 0)) {
		socketAddressInit();
		connectServer();
	}
	
	Client(int port) : server_port(port), socketServer(socket(AF_INET, SOCK_STREAM, 0)) {
		socketAddressInit();
		connectServer();
	}
	
	Client(std::string serverAddress) : serverAddress(serverAddress), socketServer(socket(AF_INET, SOCK_STREAM, 0)) {
		socketAddressInit();
		connectServer();
	}
	
	void socketAddressInit() {
		memset(&socketAddressIn, 0, sizeof(socketAddressIn));
		socketAddressIn.sin_family = AF_INET;
		socketAddressIn.sin_port = htons(server_port);
		socketAddressIn.sin_addr.s_addr = inet_addr(serverAddress.c_str());
	}
	
	int connectServer() {
		// connect to server
		int status = 0;
		if (status = connect(socketServer, (struct sockaddr *) &socketAddressIn, sizeof(socketAddressIn)) < 0) {
			perror("connect error");
			return status;
		}
		return status;
	}
	
	void sendPackage(Slice &key, Slice &value) {
		Package p(key, value);
		// send Package
		send(socketServer, (char *) &p, sizeof(Package), 0);
#ifdef DEBUG_OUTPUT
		printf("send Package: id:%u, key:%d, value:%d \n", (unsigned int) p.id,key.id,value.id);
#endif
	}

	void sendPackage(unsigned int id,Slice &key, Slice &value) {
		Package p(id,key, value);
		// send Package
		send(socketServer, (char *) &p, sizeof(Package), 0);
#ifdef DEBUG_OUTPUT
		printf("send Package: id:%u, key:%d, value:%d \n", (unsigned int) p.id,key.id,value.id);
#endif
	}
	
	Result receive() {
		// receive Result
		char recvResult[RESULT_BUFFER_SIZE];
		Result buffer;
		memset(&buffer, 0, sizeof(Result)); // clean to 0
		ssize_t recvbytes;
		if ((recvbytes = recv(socketServer, recvResult, RESULT_BUFFER_SIZE, 0)) == -1) {
			perror("recv error");
			exit(1);
		}
		memcpy(&buffer, recvResult, sizeof(Result));
#ifdef DEBUG_OUTPUT
		printf("receive Result, id:%d\n", buffer.id);
#endif
		return buffer;
	}
	
	void closeConnection() {
		close(socketServer);
#ifdef DEBUG_OUTPUT
		printf("one thread complete\n");
#endif
	}
	
	void test() {
		Slice key(0), value(0);
		sendPackage(key, value);
		auto result=receive();
#ifdef DEBUG_OUTPUT
		if(result.id!=value.id){
			std::cout<<"result.id:"<<result.id<<"\tvalue.id:"<<value.id;
			assert(result.id==value.id);
		}
#endif
	}
	
	inline void test(int keyNum, int valueNum) {
		Slice key(keyNum), value(valueNum);
		sendPackage(key, value);
		auto result=receive();
		
#ifdef DEBUG_OUTPUT
		if(result.id!=value.id){
			std::cout<<"result.id:"<<result.id<<"\tvalue.id:"<<value.id;
			assert(result.id==value.id);
		}
#endif
	}

	inline void test(unsigned int id, int keyNum, int valueNum) {
		Slice key(keyNum), value(valueNum);
		sendPackage(id, key, value);
		auto result=receive();
		
#ifdef DEBUG_OUTPUT
		if(result.id!=value.id){
			std::cout<<"result.id:"<<result.id<<"\tvalue.id:"<<value.id;
			assert(result.id==value.id);
		}
#endif
	}
	
private:
	std::string serverAddress="127.0.0.1";
	int socketServer;
	int server_port = SERVER_PORT;
	struct sockaddr_in socketAddressIn;
};

#endif //TCPEXAMPLE_CLIENT_H
