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



void producer(queue<TransferData*>* transfer_datas, mutex* m, sem_t *s, int sock) {
    while (1) {
    
		sem_wait(s); // for prevent queue max count

		TransferData *data = new TransferData;
		data->buffer = new char[1024*1024];

		int len = recv(sock, data->buffer, 1024 * 1024, 0);
		data->size = len;

        m->lock();
		transfer_datas->push(data);
        m->unlock();

		printf ("recv %d\n", data->size);  fflush (stdout);

		if (len<=0) break;
    }

	printf ("producer quiting\n");  fflush (stdout);
}


void consumer(queue<TransferData*>* transfer_datas, mutex* m, sem_t *s, FILE* fd) {

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
			fwrite(data->buffer, 1, data->size, fd);
			printf ("write %d\n", data->size);  fflush (stdout);
		}

		delete [] data->buffer;
		delete data;

 		if (r <= 0) break;
		
    }

	printf ("Consume quiting\n");  fflush (stdout);
}

void writeLog(const char *msg)
{
	printf("%s\n", msg);
}


int main(int argc, char* argv[])
{
    char errstring[1024];

   	int clientSocket;
	sockaddr_in address, clinetAddr;
	int addlen = sizeof(address);
	int option = 1;  // SO_REUSEADDR
    

	int m_ServerSocket = socket(PF_INET, SOCK_STREAM, 0);
	if (m_ServerSocket == 0)
	{
		writeLog("socket went wrong");
		return -1;
	}

	setsockopt(m_ServerSocket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

	//주소
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	address.sin_port = htons(7701); // test port 7701

	if (bind(m_ServerSocket, (sockaddr*)&address, addlen) < 0)
	{
		writeLog("socket bind error");
        close(m_ServerSocket);
		return -1;
	}

	writeLog("wait for client");

	if (listen(m_ServerSocket, 5) < 0)
	{
		writeLog("socket listen wrong");
        close(m_ServerSocket);
		return -1;
	}

   int iResult;
   struct timeval tv = { 1, 0 };  // 1 second
   fd_set rfds;

   FD_ZERO(&rfds);
   FD_SET(m_ServerSocket, &rfds);



#if 1
   clientSocket = accept(m_ServerSocket, (sockaddr*)&clinetAddr, (socklen_t*)&addlen);
#else
   // Accept에 타임아웃을 적용시킨다. 수신종료를 하는 경우에 탈출할수 있도록 1초 타임아웃을 주고 계속 반복시킨다.
   // accept 루프 도중에 ShutDownRequested에 true에 전달되면 루프를 탈출한다.

   while (!ShutDownRequested)
   {
       iResult = select(m_ServerSocket + 1, &rfds, (fd_set*)0, (fd_set*)0, &tv);
       if (iResult > 0)
       {
           clientSocket = accept(m_ServerSocket, (sockaddr*)&clinetAddr, (socklen_t*)&addlen);
           break;
       }
       else if (iResult == 0)
       {
           tv.tv_sec = 1;
           tv.tv_usec = 0;
           FD_ZERO(&rfds);
           FD_SET(m_ServerSocket, &rfds);
           continue;
       }
   }

   if (ShutDownRequested)
       return 0;

#endif

	if (clientSocket < 0)
	{
		writeLog("socket went wrong");
        close(m_ServerSocket);
		return -1;
	}

    FILE* m_fp = NULL;
    char *RecvBuff = new char[1024 * 1024 * 1024]; // 1GB




        string rawfilepath = "tmp.raw";
        printf("Open File %s\n", rawfilepath.c_str());

        m_fp = fopen(rawfilepath.c_str(), "wb");
        if (m_fp == NULL)
        {
            if (RecvBuff) delete [] RecvBuff;
            return -1;
        }


#if 0		
        while (1)
        {
            int len = recv(clientSocket, RecvBuff, 1024 * 1024 * 1024, 0);
            if (len <= 0)
                break;
            
            printf("Received %d\n", len);
			//cout << "Received " << to_string(len) << endl;

            if (len != fwrite(RecvBuff, 1, len, m_fp))
            {
              //  cout << "Write Error " << to_string(len) << endl;
            }
        }
#else
	mutex m;
	sem_t s;
	queue<TransferData*> transfer_datas;
	sem_init(&s, 0, 50);	
    thread t_tcpRecv(producer, &transfer_datas, &m, &s, clientSocket);
	thread t_fileWrite(consumer, &transfer_datas, &m, &s, m_fp);
	t_tcpRecv.join();
	t_fileWrite.join();
	sem_destroy(&s);
#endif





        fclose(m_fp);
  
    


    if (RecvBuff) delete [] RecvBuff;
    close(m_ServerSocket);

	return 0;
}

