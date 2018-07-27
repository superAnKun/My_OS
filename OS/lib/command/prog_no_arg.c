#include "stdio.h"
#include "string.h"
#include "syscall.h"

int main(int argc, char** argv) {
	int32_t fd[2] = {-1};
	pipe(fd);
	printf("mian: 111 fd[0]: %d fd[1]: %d\n", fd[0], fd[1]);
	int32_t pid = fork();
	//int pid = 1;
	char buf[32] = {0};
	if (pid) {
		write(fd[1], "Hi, my son, I love you!", 24);
		printf("\nI'm father, my pid is %d\n", getpid());
		return 8;
	} else {
		uint32_t delay = 900000;
		while (delay--);
		read(fd[0], buf, 24);
		printf("\nI'm child my pid is %d %s\n", getpid(), buf);
		printf("my father said to me : %s \n", buf);
	}
	printf("buf: %s\n", buf);
	//while (1);
	return 0;
}
