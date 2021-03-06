#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <czmq.h>
#include "Threaded Logger/logger.h"

#define PI 3.14159265

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>

#include <netinet/in.h>

#define HEADERSIZE 10
#define PORT 49417
#define TIME_STR_SIZE 27 //YYYY-mm-DD-HH-MM-SS-uuuuuu\0

size_t getTimeString(char* str_result, size_t str_size, char delim) 
{	/**
	Returns the current time (UTC) as a string of format
	YYYY-mm-dd-HH-MM-SS-UUUUUU where mm is the month number and UUUUUU are the microseconds of the epoch time
	**/

	struct timeval curr_time_tv;
	char time_str[str_size];
	char out_str[str_size];
	
	//get current time; tv provides sec and usec; tm used for strftime
	gettimeofday(&curr_time_tv, NULL); //get current time in timeval
	struct tm *curr_time_tm = gmtime(&(curr_time_tv.tv_sec)); //use sec of timeval to get struct tm

	//use tm to get timestamp string
	strftime(time_str, str_size, "%Y-%m-%d-%H-%M-%S",curr_time_tm);
	size_t true_size = snprintf(str_result, str_size, "%s-%06ld",time_str, curr_time_tv.tv_usec); //add microseconds fixed to 6-character width left-padded with zeroes
    
	//str_size is characters to write INCLUDING null terminator; if true_size == str_size, then resulting string was truncated; 
	//max of (str_size - 1) can be written
	if (true_size == str_size) true_size = str_size - 1; 
    
	//replace default '-' delimiter if needed
    if (delim != '-') 
	{
        char* ptr_found; //pointer to found substring
        while (1) 
		{
            ptr_found = strstr(str_result,"-"); //find substring
            if (ptr_found == NULL) 
			{
                break; //if NULL, then no occurences of "-" so done
            }
            else 
			{
                *ptr_found = delim; //otw replace found '-' with desired delimiter
            }
        }    
    }
    
    return true_size;
}

int main() 
{
	//print ZMQ version
	int major;
	int minor;
	int patch;
	
	//Print ZMQ Version
	/* zmq_version(&major, &minor, &patch);
	printf("ZMQ Version: %d.%d.%d\n", major, minor, patch); */

	//Initializing ZMQ sockets
	/* void *context = zmq_ctx_new();
	void *publisher = zmq_socket(context, ZMQ_PAIR); //Create PAIR socket for bi-directional, 1-to-1 communication
	if(zmq_bind(publisher,"tcp://*:49217") != 0) {
		perror("zmq_bind() error:");
	} */

	//Initializing logging thread
	logger_t* logger = loggerCreate(30);
	pthread_t logger_tid;
	pthread_create(&logger_tid, NULL, &loggerMain, logger);
	////////////////////////////////////////

	char server_message[50] = "You have reached the server!";

	// create the listening server socket
	int server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket  < 0) 
	{
		perror("socket() call failed!");
		exit(1);
	}

	// define the server address
	struct sockaddr_in server_address;
	server_address.sin_family = AF_INET; //TCP
	server_address.sin_port = htons(PORT); //define port
	server_address.sin_addr.s_addr = INADDR_ANY; //accept connection at any address available

	// bind server socket to the address
	if (bind(server_socket, (struct sockaddr*) &server_address, sizeof(server_address)) < 0) 
	{
		perror("bind() call failed!");
		exit(1);
	}
	
	
	if (listen(server_socket, 5) < 0) 
	{
		perror("listen() failed!");
		exit(1);
	}
	
	uint8_t loop_stop = false;
	while (!loop_stop)  //loop to discard dead clients and wait for a reconnection 
	{
		printf("Waiting for client connection...\n");

		int client_socket;
		client_socket = accept(server_socket, NULL, NULL);
		if (client_socket < 0) 
		{
			perror("accept() failed");
			exit(1);
		}

		//configure TCP keepalive to detect dead client
		uint8_t val = 1;
		setsockopt(client_socket, SOL_SOCKET, SO_KEEPALIVE,&val, sizeof(val));
		
		printf("Connection established\n");
			
		char time_str[TIME_STR_SIZE];
		int i = 0;
		useconds_t delay =100000; //1000 ms
		//printf("Entering loop...\n");
		while (!loop_stop)
		{
			//getTimeString(time_str, TIME_STR_SIZE, '_');
			
			//Transmitting epoch time instead
			struct timeval curr_time_tv; 
			gettimeofday(&curr_time_tv,NULL);
			sprintf
			(
				time_str,"%lu.%06lu", curr_time_tv.tv_sec, curr_time_tv.tv_usec
			);
			//printf("Got time string\n");

			double x;
			x = sin(i*PI/180.0);
			//printf("calculated sine\n");

			size_t msg_size = HEADERSIZE + 2 + TIME_STR_SIZE; 
			char msg[msg_size];
			sprintf(msg,"%0*g_%s",HEADERSIZE, x, time_str);
			size_t true_msg_size = strlen(msg);
			//printf("built msg string\n");


			//zmq socket send
			/* 
			if (zmq_send(publisher,msg, true_msg_size,ZMQ_DONTWAIT) < 0) {
				perror("zmq_send() Error:");
			}
			printf("zmq sent\n"); 
			*/

			//standard socket send
			ssize_t send_status = send(client_socket, msg, true_msg_size, MSG_NOSIGNAL);
			if (send_status >= 0) 
			{
				if(send_status < true_msg_size) 
				{
					printf("Not all data sent\n");
				}
				else 
				{
					//printf("Message sent: %s\n",msg);
				}
			}
			else if (errno == EPIPE) 
			{
				printf("sending on dead socket. Breaking from loop.\n");
				break;
			}
			else 
			{
				perror("Error in send(): ");
				break;
			}
			//printf("sent\n");

			size_t rec_size = 100;
			char received[rec_size];
			if (recv(client_socket,received, rec_size, MSG_DONTWAIT) > 0) {
				printf("Message received: %s\n", received);
				loggerSendLogMsg(logger,received,sizeof(received),"./server_logs.txt",0,false);
				printf("After log?\n");
				if (strcmp(received, "CLOSE") == 0) {
					printf("Stop Command Received\n");
					loop_stop = true;
					break;
				}
			}
			//printf("rec\n"); 
			
			/* 
			zmq_msg_t rec_msg;
			zmq_msg_init(&rec_msg); 
			if (zmq_msg_recv(&rec_msg, publisher, ZMQ_DONTWAIT) < 0) {
				if (errno != EAGAIN) perror("zmq_msg_recv() ERROR: ");
			}
			else {
				char* zmq_received = zmq_msg_data(&rec_msg);
				printf("zmq_received=%s\n",zmq_received);
				loggerSendLogMsg(logger,zmq_received,zmq_msg_size(&rec_msg),"./server_logs.txt",0,false);
				if (strcmp(zmq_received, "CLOSE") == 0) {
					printf("Stop Command Received\n");
					loop_stop = true;
				}
			}
			zmq_msg_close(&rec_msg);
			printf("zmq_req\n"); 
			*/

			i += 5;
			i = i % 360;

			usleep(delay);
		} // end while (!loop_stop)
		close(client_socket);	
	} // end while (1)
	loggerSendCloseMsg(logger,0,true);
	pthread_join(logger_tid,NULL);
	// zmq_close(publisher);
	// zmq_ctx_destroy(context);
	close(server_socket);
	printf("Exitted\n");
	return 0;
}
