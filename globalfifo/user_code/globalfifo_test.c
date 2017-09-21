#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

static void signalio_handler(int signum)
{
	printf("receive a signal from globalfifo, signalnum:%d\n", signum);
}

void main(void)
{
	int fd,oflags;
	printf("in the function main()\n");
	fd = open("/dev/globalfifo", O_RDWR, S_IRUSR | S_IWUSR);
	printf("after open device,fd = %d\n",fd);
	if(fd != -1){
		printf("open /dev/globalfifo device is ok\n");
		signal(SIGIO, signalio_handler);
		fcntl(fd, F_SETOWN, getpid());
		oflags = fcntl(fd, F_GETFL);
		fcntl(fd, F_SETFL, oflags | FASYNC);
		while(1){
			sleep(100);
		}
	}else{
		printf("device open failure\n");
	}
}
