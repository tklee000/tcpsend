#include <string>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdint.h>
#include <syslog.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/poll.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <signal.h> 
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#include <mutex>
#include <thread>
#include <queue>
#include <string>

using namespace std;

#define TRUE (1)
#define FALSE (!TRUE)


typedef int32_t SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;

#define CLOSE_SOCKET(socket) (close(socket))

#define INVALID_SOCKET (~0)
#define SOCKET_ERROR (-1)

using namespace std;


typedef struct _TransferData
{
    char *buffer;
	int size;
} TransferData;



void producer(queue<TransferData*>* transfer_datas, mutex* m, sem_t *s, int fd) {
    while (1) {

		sem_wait(s); // for prevent queue max count

		TransferData *data = new TransferData;

		data->buffer = new char[1024*1024];
		ssize_t r = read(fd, data->buffer, 1024*1024);
		data->size = r;

        m->lock();
		transfer_datas->push(data);
        m->unlock();

		printf ("read %d\n", data->size);  fflush (stdout);

		if (r<=0) break;
    }

	printf ("producer quiting\n");  fflush (stdout);
}


void consumer(queue<TransferData*>* transfer_datas, mutex* m, sem_t *s, int sock) {

    while (1) {

        m->lock();
        if (transfer_datas->empty()) {
            m->unlock();
             this_thread::sleep_for(chrono::milliseconds(10));
            continue;
        }
        
        TransferData *data = transfer_datas->front();
        transfer_datas->pop();
        m->unlock();

		sem_post(s);

		int r = data->size;
 		if (r)
		{
			send(sock,data->buffer,data->size,0);
			printf ("send %d\n", data->size);  fflush (stdout);
		}

		delete [] data->buffer;
		delete data;

 		if (r <= 0) break;
		
    }

	printf ("Consume quiting\n");  fflush (stdout);
}


int main(int argc, char* argv[])
{
	vector<string> inputfiles;
	string outputfile;
	string csvfile;
	


	int32_t Result;
	SOCKADDR_IN servAddr;

	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = inet_addr("192.168.118.140");
	servAddr.sin_port = htons(7701);

	int ClntSocket = socket(PF_INET, SOCK_STREAM, 0);

	if (connect(ClntSocket, (SOCKADDR*)&servAddr, sizeof(servAddr)) == SOCKET_ERROR) {
		CLOSE_SOCKET(ClntSocket);
		printf("not Connected RSG_DMM 7701\n");
		return 0;
	}

	printf("Connected RSG_DMM 192.168.10.25:7701\n");

	int fd;
	int rc;
	char errstring[1024];
	
	printf("Open File /home/tklee/TcpSendDummy/1.mp4\n");

	fd = open("/home/tklee/TcpSendDummy/1.mp4", O_RDONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH );
	if(fd == -1){
		sprintf(errstring, "open Error (%s)\n", "/home/tklee/TcpSendDummy/1.mp4");
		perror(errstring);
		return -1;
	}

	struct timespec begin, end;
	clock_gettime(CLOCK_MONOTONIC, &begin);

#if 1
	mutex m;
	sem_t s;
	queue<TransferData*> transfer_datas;
	sem_init(&s, 0, 50);
    thread t_fileread(producer, &transfer_datas, &m, &s, fd);
	thread t_tcpSend(consumer, &transfer_datas, &m, &s, ClntSocket);
	t_fileread.join();
	t_tcpSend.join();
	sem_destroy(&s);

#else
	printf("begin read\n");
	while (1)
	{
		ssize_t r = read(fd, rbuff, sizeof(rbuff));
//		printf("read %ld\n", r);

		if (r<=0) break;

		send(ClntSocket,rbuff,r,0);
//		printf("Sent %ld\n", r);
	}
	printf("end read\n");
#endif
	clock_gettime(CLOCK_MONOTONIC, &end);
	float total_time = 1000000000 * (end.tv_sec - begin.tv_sec) + (end.tv_nsec - begin.tv_nsec);
	printf("time=%.2f s\n", total_time);


	close(fd);
	close(ClntSocket);



	return 0;
}

