
#ifndef TCPEXAMPLE_DATA_H
#define TCPEXAMPLE_DATA_H

#include <cstdint>
#include <atomic>

#define PACKAGE_BUFFER_SIZE 128
#define RESULT_BUFFER_SIZE 80
#define SERVER_PORT    6666

class Slice {
public:
	int id;
	
	Slice() : id(0) {};
	
	Slice(int id) : id(id) {};
};

class Package {
public:
	unsigned int id;
	Slice key;
	Slice value;
	
	Package(Slice key, Slice value) : key(key), value(value),
	                                  id(0) {};
	
	Package(unsigned int id, Slice key, Slice value) : key(key), value(value), id(id) {};
	
	Package() : id(0) {};
	
	Package(unsigned int id) : id(id) {};
};

class Result {
public:
	int id = 0;
	
	Result(int result) : id(result) {};
	
	Result() : id(0) {};
};


struct ProcessStatus {
	bool ready_to_receive_;
	bool ready_to_send_;
	
	ProcessStatus() : ready_to_receive_(true), ready_to_send_(true) {};
	
	ProcessStatus(bool receive, bool send) : ready_to_receive_(receive), ready_to_send_(send) {};
};

#endif //TCPEXAMPLE_DATA_H
