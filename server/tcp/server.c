/* File Name: server.c */  
#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  
#include <errno.h>  
#include <signal.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>  
#include <sys/socket.h>  
#include <netinet/in.h>  
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>

#define gettid() syscall(__NR_gettid)


#define DEFAULT_PORT 9600
#define MAXLINE 1024
#define MAXIDS 1024

struct pthread_infos_ {
	char used;
	int fd;
	pthread_t id;
};

struct pthread_infos_ pthread_infos[MAXIDS];

int g_exit = 0;

static void sig_int(int signo);
static void client_thread(void *v);
static int select_id(void);
static void get_current_time(char *buf);
static void out_logs(char *buf, int len, int cfd);
static void out_log(char *buf, int len, int cfd, char newline);

int main(int argc, char** argv)  
{  
	struct sockaddr_in     servaddr;  
//	char    buff[4096];  
	int     n;  
	int    socket_fd, connect_fd;  
	//pthread_t ids[MAXIDS];  

	memset(pthread_infos, 0, sizeof(pthread_infos));

	if(signal(SIGINT, sig_int) == SIG_ERR)
		perror("main: can't catch SIGINT");

	//初始化Socket  
	if( (socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1 ){  
		printf("create socket error: %s(errno: %d)\n",strerror(errno),errno);  
		exit(0);  
	}
	n = 1;
	if (setsockopt( socket_fd, SOL_SOCKET, SO_REUSEADDR, &n, sizeof(n) ) == -1) {
		printf("setsockopt error:");
		perror(strerror(errno));
		exit(0);
	}
	int flags = fcntl(socket_fd, F_GETFL, 0);
	if (0 != fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK)) {
		printf("set socket_fd to NO block fail: %s\n", strerror(errno));
		exit(0);
	}
	//初始化  
	memset(&servaddr, 0, sizeof(servaddr));  
	servaddr.sin_family = AF_INET;  
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);//IP地址设置成INADDR_ANY,让系统自动获取本机的IP地址。  
	servaddr.sin_port = htons(DEFAULT_PORT);//设置的端口为DEFAULT_PORT  

	//将本地地址绑定到所创建的套接字上  
	if( bind(socket_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1){  
		printf("bind socket error: %s(errno: %d)\n",strerror(errno),errno);  
		exit(0);  
	}  
	//开始监听是否有客户端连接  
	if( listen(socket_fd, 10) == -1){  
		printf("listen socket error: %s(errno: %d)\n",strerror(errno),errno);  
		exit(0);  
	}  
	printf("======waiting for client's request======\n");  
	while(g_exit == 0){  
		//阻塞直到有客户端连接，不然多浪费CPU资源。  
		if( (connect_fd = accept(socket_fd, (struct sockaddr*)NULL, NULL)) == -1){  
			if (EAGAIN == errno) {
				usleep(500);
				continue;
			 } else {
				printf("accept socket error: %s(errno: %d)",strerror(errno),errno);  
				g_exit = 0;
				continue;
			}
		}

		int i;
		i = select_id();
		if (-1 == i) {
			close(connect_fd);
			usleep(50);
			continue;
		}
		pthread_infos[i].used = 1;
		pthread_infos[i].fd = connect_fd;

		//接受客户端传过来的数据  
		if (pthread_create(&pthread_infos[i].id, NULL, (void *)client_thread, (void *)&pthread_infos[i]) != 0) {
			printf("ptread_create error!!\n");
		} else {
			pthread_infos[i].used = 0;
		}
		
	}
	int i;
loop_exit:
	for (i = 0; i < MAXIDS; i++) {
		if (pthread_infos[i].used == 1)
			break;
	}
	if (i < MAXIDS) {
		usleep(100);
		goto loop_exit;
	}
	close(socket_fd);  
	printf("server exit\n");
}

static void sig_int(int signo)
{
	g_exit = 1;
}
 
static void client_thread(void *v)
{
	#if 0
	int n;
	int cfd = *((int *)v);
	unsigned char buf[MAXLINE];

	while (g_exit == 0) {
		n = recv(cfd, buf, MAXLINE, 0);  
		if (n <= 0) {
			break;
		} else {
			int i;
			printf("\n[%d] ", cfd);
			for (i = 0; i < n; i++)
				printf("%c", buf[i]);
			fflush(stdout);
		}
		usleep(50);
	}
	close(cfd);
	#else
	#define SAVE_PATH "/run/shm/"
	int n, value = 0;
	//int cfd = *((int *)v);
	struct pthread_infos_ *pth = (struct pthread_infos_ *)v;
	int cfd = pth->fd;
	unsigned char buf[MAXLINE];
	fd_set rflags, wflags;
	struct timeval waitd;
	struct timeval s_timer;
	struct timeval s_timer2;
	struct timeval tv;
	struct timezone tz;

	char light = 1;
	int voice_flag = 2;

	gettimeofday(&s_timer, &tz);
	gettimeofday(&s_timer2, &tz);
	
	sprintf(buf, "echo \"cfd:%d\" >> %scfd", cfd, SAVE_PATH);
	system(buf);

	while (g_exit == 0) {
		waitd.tv_sec = 0;
		waitd.tv_usec = 100;
		FD_ZERO(&rflags);
		FD_ZERO(&wflags);
		FD_SET(cfd, &rflags);
		FD_SET(cfd, &wflags);
		int e = select(cfd + 1, &rflags, &wflags, 0, &waitd);
		if (e < 0) {
			if (EINTR == errno) {
				continue;
			} else {
				break;
			}
		}
		if (FD_ISSET(cfd, &rflags)) {
			n = recv(cfd, buf, MAXLINE, 0);  
			if (n == 0) {
				break;
			}
			else if (n < 0) {
				if (EAGAIN == errno || EWOULDBLOCK == errno || EINTR == errno) {

				} else {
					printf("\n[%u] [%u] [%d] read : ", gettid(), getpid(), cfd);
					perror(strerror(errno));
					//printf("\n[%d]!!! read error!!!\n", cfd);
					break;
				}
			} else {
				int i;
				#if 0
				printf("\n[%d] ", cfd);
				for (i = 0; i < n; i++)
					printf("%c", buf[i]);
				fflush(stdout);
				#elif 0
				/*char lbuf[MAXLINE * 2];
				char time[64];
				get_current_time(time);
				sprintf(lbuf, "echo \"%s [%d] ", time, cfd);
				//system(lbuf);
				//strcpy(lbuf + strlen(lbuf), "echo \"");
				for (i = 0; i < n; i++) {
					sprintf(lbuf + strlen(lbuf), "%c", buf[i]);
					if (buf[i] == '#' && i < n - 1)
						strcat(lbuf, "\n");
				}
				sprintf(lbuf + strlen(lbuf), "\" >> %s/msg", SAVE_PATH);
				system(lbuf);*/
				#else
				out_logs(buf, n, cfd);
				#endif
			}
		}
		if (FD_ISSET(cfd, &wflags)) {
			gettimeofday(&tv, &tz);
			if (tv.tv_sec >= s_timer.tv_sec + 2) {
				int i;
				s_timer.tv_sec = tv.tv_sec;
				//strcpy((char *)buf, "ST,GS,+ 001440kg\x00D\x00A");
				//strcpy((char *)buf, "T:I:123456,3432#");
				//sprintf((char *)buf, "T:I:123456,02030405060708090A0B0C0D0F101112131415161718191A1B1C1D1F#");
				//sprintf((char *)buf, "T:I:123456,02#");
				
				/*buf[0] = '\0';
				strcat(buf + strlen(buf), "T:I:12,");
				for (i = voice_flag; i < voice_flag + 10; i++) {
					if (i == 108) {
						i = 2;
						break;
					}
					sprintf((char *)buf + strlen(buf), "%.2X", i);
				}
				voice_flag = i;
				strcat(buf + strlen(buf), "#");*/

				//strcpy(buf, "T:I:12,4a050f4201345140#");

				sprintf((char *)buf, "T:L:1234,0,%d#", value % 2);
				value++;
				if ((i = send(cfd, buf, strlen(buf), 0)) == -1) {
					printf("\n[%d] write : ", cfd);
					perror(strerror(errno));
					break;
				}
				if (i < strlen(buf))
					printf("send low\n");
			}
			/*if (tv.tv_sec >= s_timer2.tv_sec + 3) {
				if (light == 1)
					light = 2;
				else
					light = 1;
				s_timer2.tv_sec = tv.tv_sec;
				//strcpy((char *)buf, "ST,GS,+ 001440kg\x00D\x00A");
				//strcpy((char *)buf, "T:I:123456,3432#");
				sprintf((char *)buf, "T:J:%d,1,%d#", light, light);
				//printf("[%d] %s\n", cfd, buf);
				if (send(cfd, buf, strlen(buf), 0) == -1) {
					printf("\n[%d] write : ", cfd);
					perror(strerror(errno));
					break;
				}
			}*/
		}

		usleep(50);
	}
	close(cfd);
	pth->used = 0;
	
	printf("[%d] exit\n", cfd);
	#endif
}

static int select_id(void)
{
	int i;

	for (i = 0; i < MAXIDS; i++) {
		if (pthread_infos[i].used == 0)
			break;
	}

	if (i >= MAXIDS)
		i = -1;

	//return &pthread_infos.ids[i];
	return i;
}

static void get_current_time(char *buf)
{
	time_t currenttime;
	struct tm *time1;

	time(&currenttime);
	time1 = localtime(&currenttime);
	sprintf(buf, "[%d:", time1->tm_year + 1900);
	sprintf(buf + strlen(buf), "%d:", time1->tm_mon + 1);
	sprintf(buf + strlen(buf), "%d:", time1->tm_mday);
	sprintf(buf + strlen(buf), "%d:", time1->tm_hour);
	sprintf(buf + strlen(buf), "%d:", time1->tm_min);
	sprintf(buf + strlen(buf), "%d]", time1->tm_sec);
}

static void out_logs(char *buf, int len, int cfd)
{
	int i, j = 0;
	//char time[64];
	//char out[MAXLINE * 2];

	//get_current_time(time);
	//sprintf(out, "echo \"%s [%d] ", time, cfd);
	/*printf("read:");
	for (i = 0; i < len; i++) {
		printf("%c", buf[i]);
	}
	printf("\n");*/
	for (i = 0; i < len; i++) {
		#if 0
		sprintf(out + strlen(out), "%c", buf[i]);
		if (buf[i] == '#' && i < len - 1)
			strcat(out, "\n");
		#else
		if (buf[i] == '#') {
			char f = 0;;
			out_log(buf + j, i + 1 - j, cfd, f);
			j = i + 1;
		}
		#endif
	}
	//sprintf(out + strlen(out), "\" >> %s/msg", SAVE_PATH);
	//system(out);
}

static void out_log(char *buf, int len, int cfd, char newline)
{
	//int i, j = 0;
	int l;
	char time[64];
	char out[MAXLINE * 2];

	get_current_time(time);
	sprintf(out, "echo \"%s [%d] ", time, cfd);
	l = strlen(out);
	memcpy(out + l, buf, len);
	l += len;
	if (newline) {
		out[l] = '\n';
		l++;
	}
	out[l] = '\0';
	sprintf(out + strlen(out), "\" >> %s/msg", SAVE_PATH);
	system(out);
}
