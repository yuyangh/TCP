#include <iostream>
#include <cstdlib>
#include <thread>
#include <vector>
#include <functional>
#include <pthread.h>
#include "client.h"
#include <atomic>
#include <math.h>
#include <iostream>
#include <ratio>
#include <chrono>  // for high_resolution_clock

using namespace std;

//-------------------------------------------------
// GLOBAL VARIABLES
//-------------------------------------------------
static const int NUM_THREADS = 1000;
static const int NUM_TASK_PER_THREAD = 1000;
static const int NUM_ENTRIES = (NUM_THREADS * NUM_TASK_PER_THREAD);

static std::atomic<int64_t> ENTRY_COUNT(0);
static std::atomic<bool> END_STAT(false);
static std::atomic<unsigned long long> DURATION(0);
static std::atomic<unsigned long> UNIQUE_COUNTER(-1);
// just create an object
static auto startTime = std::chrono::high_resolution_clock::now();

//-------------------------------------------------
// FUNCTION PROTOTYPES
//-------------------------------------------------

void outputThreadID();

void thread_task(Client client, int key, int value);


int main(int argc, char **argv) {
	printf("start %s \n", argv[0]);
	
	int serverPort = SERVER_PORT;
	if (argc > 1) {
		int serverPort = atoi(argv[1]);
	}
	
	// creates clients
	std::vector<Client> clients;
	clients.reserve(NUM_THREADS);
	for (int i = 0; i < NUM_THREADS; i++) {
		clients[i] = Client(serverPort);
	}
	
	std::vector<std::thread> threadPool;
	
	// Record start time
	startTime = std::chrono::high_resolution_clock::now();
	
	for (int i = 0; i < NUM_THREADS; ++i) {
		// func(clients[i],i,i);
		threadPool.push_back(std::thread(thread_task, (clients[i]), i, i));
		if (threadPool.back().joinable()) {
			threadPool.back().detach();
		}
	}
	
	// wait until all testings has been conducted
	// then output statistics
	while (true) {
		// output statistics
		if (ENTRY_COUNT >= NUM_ENTRIES) {
			auto finishTime = std::chrono::high_resolution_clock::now();
			// Record duration time
			std::chrono::duration<double> elapsed = finishTime - startTime;
			std::chrono::microseconds duration = std::chrono::duration_cast<std::chrono::microseconds>(elapsed);
			DURATION += duration.count();
			
			
			printf("\n Output Statistics \n");
			// std::this_thread::sleep_for(std::chrono::milliseconds(100));
			std::cout << "NUM_ENTRIES:\t" << NUM_ENTRIES << "\n";
			std::cout << "NUM_THREADS:\t" << NUM_THREADS << "\n";
			std::cout << "Duration:\t" << (DURATION) << " microseconds" << endl;
			std::cout << "Elapsed time:\t" << (DURATION) / pow(10, 6) << " s" << std::endl;
			std::cout << "QPS:\t\t" << static_cast<double>(NUM_ENTRIES) / (DURATION / pow(10, 6)) << std::endl;
			break;
		}
	}
	
	return 0;
}

void outputThreadID() {
	std::cout << "From Thread ID : " << std::this_thread::get_id() << "\n";
}

void thread_task(Client client, int key, int value) {
	for (int i = 0; i < NUM_TASK_PER_THREAD; i++) {
		if (ENTRY_COUNT > NUM_ENTRIES && !DURATION) {
			break;
		}
		// send and receive
		client.test(key, ++UNIQUE_COUNTER);
		
		++ENTRY_COUNT;
	}
	client.closeConnection();
}