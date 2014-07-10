#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>  

#define PORT		9600
#define MAXDATASIZE	100

static char g_exit = 0;

static void sig_int(int signo);

int main()
{
	int sockfd;
	struct sockaddr_in server;
	struct sockaddr_in client;
	socklen_t addrlen;
	int num;
	char buf[MAXDATASIZE];

	if(signal(SIGINT, sig_int) == SIG_ERR)
		perror("main: can't catch SIGINT");

	if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) 
	{
		perror("Creatingsocket failed.");
		exit(1);
	}

	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &num, sizeof(num)) == -1) {
		printf("setsockopt error:");
		perror(strerror(errno));
		exit(0);
	}

	bzero(&server,sizeof(server));
	server.sin_family=AF_INET;
	server.sin_port=htons(PORT);
	server.sin_addr.s_addr= htonl (INADDR_ANY);
	if(bind(sockfd, (struct sockaddr *)&server, sizeof(server)) == -1)
	{
		perror("Bind()error.");
		exit(1);
	}   

	addrlen=sizeof(client);
	while(g_exit == 0)  
	{
		num =recvfrom(sockfd, buf, MAXDATASIZE, 0, (struct sockaddr*)&client, &addrlen);                                   
		if (num < 0)
		{
			perror("recvfrom() error\n");
			exit(1);
		}

		if (0 == num) {
			/* The return value will be 0 when the peer has performed an orderly shutdown.*/
			continue;
		}

		buf[num] = '\0';
		printf("You got a message (%s%) from client.\nIt's ip is%s, port is %d.\n",buf,inet_ntoa(client.sin_addr),htons(client.sin_port)); 
		sendto(sockfd,"Welcometo my server.\n",22,0,(struct sockaddr *)&client,addrlen);
		if(!strcmp(buf,"bye"))
			break;
	}
	close(sockfd);  
}

static void sig_int(int signo)
{
	g_exit = 1;
}
