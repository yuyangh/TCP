
#include "client.h"
/*
void *PrintHello(void *threadid) {
	long tid;
	tid = (long) threadid;
	cout << "Hello World! Thread ID, " << tid << endl;
	pthread_exit(NULL);
}

void pthread(){
	pthread_t threads[NUM_THREADS];
	int rc;
	long i;
	
	for (i = 0; i < NUM_THREADS; i++) {
		cout << "main() : creating thread, " << i << endl;
		rc = pthread_create(&threads[i], NULL, PrintHello, (void *) i);
		
		if (rc) {
			cout << "Error:unable to create thread," << rc << endl;
			exit(-1);
		}
	}
	pthread_exit(NULL);
}
 */

int main() {
	Client client;
	Slice key(0), value(1);
	client.sendPackage(key, value);
	client.receive();
	client.closeConnection();
	return 0;
}