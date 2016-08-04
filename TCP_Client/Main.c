/*
 C ECHO client example using sockets
 */
#include <stdlib.h>
#include <stdio.h> //printf
#include <stdint.h>
#include <string.h>    //strlen
#include <sys/socket.h>    //socket
#include <arpa/inet.h> //inet_addr
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

//get keyboard mess
int readKeyboard(unsigned char *buffer);
void *recvThread(void *sock);
void handlePacketFromZigbeeNode(unsigned char *data, int length);
int sendRequestData(char *Addr, int sock);
static char flag_ACK = 0;
char flag_stop_thread = 0;
char flag_socket_closed = 0;
char numb_packet = 0;
unsigned char message[1000], server_reply[2000] = { 0 };
FILE *f, *d;
void printMenu() {
	//system("clear");
	puts("please select function:");
	puts("1: setting socket");
	puts("2: open socket");
	puts("3: exit");
}
int main() {
	int sock, port = 4096, nread = 0;
	char ip_addr[100] = { 0 }; //, get_menu = 0;
	struct sockaddr_in server;
	struct timeval tv;
	pthread_t recv_thread;
	void* thread_ret;

	printf("input server addr: \n");
	gets(ip_addr);
	printf("input port: \n");
	scanf("%d", &port);
	if (strlen(ip_addr) == 0)
		memcpy(ip_addr, "192.168.204.247", 15);
	if (port == 0)
		port = 4096;
	//Create socket
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		printf("Could not create socket");
	}
	printf("Socket created, sock identify: %d \n", sock);
	server.sin_addr.s_addr = inet_addr(ip_addr); //convert string to long 4 bytes.
	server.sin_family = AF_INET;
	server.sin_port = htons(port);

	//Connect to remote server
	if (connect(sock, (struct sockaddr *) &server, sizeof(server)) < 0) {
		perror("connect failed. Error");
		return 1;
	}
	//Ok client is connected to server, we going to main program
	//here
	puts("Connected\n");
	tv.tv_sec = 0;
	tv.tv_usec = 80000;
	//set time to receive
	if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *) &tv,
			sizeof(struct timeval)) < 0)
		perror("setsockopt failed \n");

	//we create a thread that listen data receive from server
	//in main program we just care about sending data

	pthread_create(&recv_thread, NULL, recvThread, (void *) sock);
	//keep communicating with server

	while (1) {
		//This function return a string get from keyboard
		nread = readKeyboard(message);

		if (nread > 0) {

			printf("mess is: %s ", message);
			if (strstr((char *) &message, "quit") != NULL) {
				flag_stop_thread = 1;
				break;
			}

			//
			//check if user want to send at command to gateway
			//
			if (strstr((char *) &message, "at") != NULL)
				message[strlen((char *) &message)] = 0x0D;
			if (send(sock, message, strlen((char *) &message), 0) < 0) {
				perror("Send failed");
				return 1;
			}
			if (strstr((char *) &message, "get") != NULL) {

				sendRequestData("9C48", sock);
			}
			nread = 0;
		}
		if (flag_socket_closed) {
			flag_socket_closed = 0;
			break;
		}

	}
	pthread_join(recv_thread, &thread_ret);
	printf("close socket %d \n", sock);
	close(sock);
	fclose(f);
	fclose(d);
	return 0;
}

int readKeyboard(unsigned char *buffer) {
	//char buffer[128];
	int result, nread = 0;
	fd_set inputs, testfds;
	struct timeval timeout;
	FD_ZERO(&inputs);
	FD_SET(0, &inputs);
	testfds = inputs;
	timeout.tv_sec = 3;
	timeout.tv_usec = 500000;
	result = select(FD_SETSIZE, &testfds, (fd_set*) 0, (fd_set*) 0, &timeout);

	switch (result) {
	case 0:
		//printf("timeout \n");
		break;
	case -1:
		perror("select error");
		return -1;
		break;
	default:
		if (FD_ISSET(0, &testfds)) {
			ioctl(0, FIONREAD, &nread);
			if (nread == 0) {
				printf("keyboard done \n");
				return -1;
			}
			nread = read(0, buffer, nread);
			buffer[nread] = 0;
			//printf("read %d from keyboard: %s \n", nread, buffer);
		}
		break;
	}

	return nread;
}

int sendRequestData(char *Addr, int sock)
{
	unsigned char cmd_rqt_data[100] = {0};
	int index;
	printf("send request data \n");

	memcpy(cmd_rqt_data, "AT+UCASTB:06,", 13);
	index = strlen(cmd_rqt_data);
	cmd_rqt_data[index++] = Addr[0];
	cmd_rqt_data[index++] = Addr[1];
	cmd_rqt_data[index++] = Addr[2];
	cmd_rqt_data[index++] = Addr[3];
	cmd_rqt_data[index++] = 0x0D;
//	cmd_rqt_data[index++] = 0x0A;
//	cmd_rqt_data[index++] = 0x00;
	printf("command is: %s \n", cmd_rqt_data);
	send(sock, cmd_rqt_data, strlen((char *) &cmd_rqt_data), 0);
	sleep(1);
	cmd_rqt_data[index++] = 0xAA;
	cmd_rqt_data[index++] = 0x01;
	cmd_rqt_data[index++] = 0x00;
	cmd_rqt_data[index++] = 0x02;
	cmd_rqt_data[index++] = 0xC1;
	cmd_rqt_data[index++] = 0xC2;
	cmd_rqt_data[index++] = 0x0D;
	send(sock, &cmd_rqt_data[18], 6, 0);
	return 1;
}

void *recvThread(void *sock) {
	int nrev = 0, i, len_rev = 0;
//	struct timeval begin, stop;
//    static char flag_count_timer = 0;
//    char rev_bcast[200] = {0};
	char *buffptr = NULL;
	time_t mytime;
	mytime = time(NULL);
	//create file and save log data
	f = fopen("log.txt", "a");
	d = fopen("draft.txt", "a");
	if (f == NULL) {
		printf("could not open file, data may be lost \n");
	}
	//write time start program:
	fprintf(f, "%s", ctime(&mytime));
	fprintf(d, "%s", ctime(&mytime));

	while (1) {

		bzero(server_reply, sizeof(server_reply)); // clear buffer;
		buffptr = (char *) &server_reply[0];
		//while ((nrev = recv((int *) sock, buffptr, server_reply + sizeof(server_reply) - buffptr - 1, 0)) > 0)
		while ((nrev = recv((int *) sock, buffptr, 1, 0)) > 0) //read byte per byte
		{
			buffptr += nrev;
			if (strlen((char *) &server_reply[0]) > 4) {
				if (*(buffptr - 1) == '\n' && *(buffptr - 2) == '\r')
					break;
			}
			if (flag_stop_thread == 1) //quit program
					{
				printf("ok quit, wait data here: %d  %s\n",
						strlen((char *) &server_reply[0]), server_reply);
				break;
			}
		}

		if (nrev > 0 && strlen((char *) &server_reply[0])) { //buffer has data

			printf("rev: %d :%s", strlen((char *) &server_reply[0]),
					server_reply);
			if (strstr((char *) &server_reply[0], "UCAST") != NULL) {

				len_rev = (int) (strtol((char *) &server_reply[25], NULL, 16));
				//fprintf(f, "%s", server_reply); //write to log file
				//fwrite(server_reply, 1, strlen((char *)&server_reply[0]), f);
				//fflush(f);
				handlePacketFromZigbeeNode(&server_reply[29], len_rev);
				printf("len_rev: %d \n", len_rev);
				for(i = 0; i < len_rev; i++)
				{
					printf("%02X ", server_reply[29+i]);
				}
				printf("\n\n");

				//this code using for counting time excute receive data
				/*				if (flag_count_timer == 0) {
				 gettimeofday(&begin, NULL);
				 flag_count_timer = 1;
				 }if ((flag_count_timer == 1)
				 && (strstr(server_reply, "done") != NULL)) {
				 gettimeofday(&stop, NULL);
				 double timetaken = (double) (stop.tv_usec - begin.tv_usec)
				 / 1000000 + (double) (stop.tv_sec - begin.tv_sec);
				 printf("time is: %f\n", timetaken);
				 flag_count_timer = 0;
				 }*/
			} else {
				fprintf(d, "%s", server_reply); //write to draft file
				fflush(d);
			}
			nrev = -1;
		} else {
			if (nrev == 0) //determine server has closed socket
					{
				printf("socket has closed \n");
				flag_socket_closed = 1;
				break; // to close socket
			}
		}
		if (flag_stop_thread == 1) {
			printf("close thread \n");
			pthread_exit(NULL);
		}
	}
}
void handlePacketFromZigbeeNode(unsigned char *data, int length) {
	char len, packet;
	char tempt_buff[100] = { 0 };
	if (data[0] == 0xBB) {
		len = data[1];
		packet = data[2];
		if (len == 0x01 && packet == 0x00) {
			printf("\nfirst packet, get number packet receive \n");
			numb_packet = data[3];
		} else {
			if (packet == 1) //header of data application
					{
				memcpy(tempt_buff, &data[7], len - 4);
				printf("%d-tempt: %s\n", packet, tempt_buff);
				fwrite(tempt_buff, 1, strlen((char *) &tempt_buff[0]), f);
				fflush(f);
			} else if (packet > 1) {
				bzero(tempt_buff, sizeof(tempt_buff));

				printf("ok read %d bytes \n", len);
				if(numb_packet != packet)
				{
					memcpy(tempt_buff, &data[3], len );
					fwrite(tempt_buff, 1, strlen(tempt_buff), f);
				}
				else
				{
					memcpy(tempt_buff, &data[3], len - 2); //drop 2 byte CRC of data user
					fwrite(tempt_buff, 1, strlen((char *) &tempt_buff[0]), f);
				}
				fflush(f);
			}
		}
	} else {
		printf("not Zigbee node command: %02X \n", data[0]);
	}
}
