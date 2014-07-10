#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>  
#include <signal.h>
#include <fcntl.h>

#define PORT		9600
#define MAXDATASIZE	100

static char g_exit = 0;

static void sig_int(int signo);

int main(int argc, char *argv[])
{
	int sockfd, num;
	char buf[MAXDATASIZE];

	struct hostent *he;
	struct sockaddr_in server,peer;

	if (argc !=3)
	{
		printf("Usage: %s <IP Address><message>\n",argv[0]);
		exit(1);
	}

	if ((he=gethostbyname(argv[1]))==NULL)
	{
		printf("gethostbyname()error\n");
		exit(1);
	}

	if(signal(SIGINT, sig_int) == SIG_ERR)
		perror("main: can't catch SIGINT");

	if ((sockfd=socket(AF_INET, SOCK_DGRAM, 0))==-1)
	{
		printf("socket() error\n");
		exit(1);
	}

	if (setsockopt( sockfd, SOL_SOCKET, SO_REUSEADDR, &num, sizeof(num) ) == -1) {
		printf("setsockopt error:");
		perror(strerror(errno));
		exit(1);
	}

	int flags = fcntl(sockfd, F_GETFL, 0);
	if (0 != fcntl(sockfd, F_SETFL, flags | O_NONBLOCK)) {
		printf("set socket_fd to NO block fail: %s\n", strerror(errno));
		exit(1);
	}

	bzero(&server, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = htons(PORT);
	server.sin_addr= *((struct in_addr *)he->h_addr);


	socklen_t  addrlen;

	while (g_exit == 0)
	{
		addrlen = sizeof(server);

		sendto(sockfd, argv[2],strlen(argv[2]), 0, (struct sockaddr *)&server, sizeof(server));

		if((num = recvfrom(sockfd, buf, MAXDATASIZE, 0, (struct sockaddr *)&peer, &addrlen)) == -1)
		{
			if (EWOULDBLOCK == errno || EAGAIN == errno) {
				printf("again\n");
				goto lp_sleep;
			} else if (ECONNREFUSED == errno) {
				printf("error recvfrom: ");
				perror(strerror(errno));
			} else {
				printf("recvfrom() error\n");
				exit(1);
			}
		}

		if (0 == num) {
			/* The return value will be 0 when the peer has performed an orderly shutdown. */
			goto lp_sleep;
		}

		if (addrlen != sizeof(server) || memcmp((const void *)&server, (const void *)&peer, addrlen) != 0)
		{
			printf("Receive message from otherserver.\n");
			continue;
		}

		buf[num]='\0';
		printf("Server Message:%s\n",buf);
lp_sleep:
		sleep(3);
	}

	close(sockfd);
}

static void sig_int(int signo)
{
	g_exit = 1;
}
