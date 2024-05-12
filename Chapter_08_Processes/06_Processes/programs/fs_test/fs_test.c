/*! Hello world program */

#include <stdio.h>
#include <lib/string.h>

char PROG_HELP[] = "Filesystem demo test.";


void createWriteTwoRead(){
	printf("----createWriteTwoRead-----\n");

	//create, write, read content with two read files open
	int fd = open("/simplefs/test", O_CREAT | O_WRONLY, 0);
	printf("fd=%d\n", fd);
	int retval = write(fd, "neki tekst", 11);
	printf("retval=%d\n", retval);
	retval = close(fd);
	printf("retval=%d\n", retval);

	fd = open("/simplefs/test", O_RDONLY, 0);
	printf("fd=%d\n", fd);
	char buff[11];
	retval = read(fd, buff, 11);
	printf("retval=%d\n", retval);
	printf("buff=%s\n", buff);

	int fd2 = open("/simplefs/test", O_RDONLY, 0);
	printf("second fd=%d\n", fd2);
	retval = read(fd2, buff, 11);
	printf("retval=%d\n", retval);
	printf("buff=%s\n", buff);

	close(fd);
	close(fd2);

	printf("\n");
}

void rewriteNoNewBlock(){
	printf("----rewriteNoNewBlock-----\n");

	//rewrite without new block allocation
	int fd = open("/simplefs/test", O_WRONLY, 0);
	write(fd, "neki tekstttt", 14);
	close(fd);

	char buf[14];
	fd = open("/simplefs/test", O_RDONLY, 0);
	read(fd, buf, 14);
	printf("buff=%s\n", buf);
	close(fd);

	printf("\n");
}

void rewriteNewBlocks(){
	printf("----rewriteNewBlocks-----\n");
	//rewrite with new block allocation
	char buf[500];
	memset(buf, 'a', sizeof(buf));
	buf[sizeof(buf) - 1] = '\0';
	int fd = open("/simplefs/test", O_WRONLY, 0);
	int retval = write(fd, buf, sizeof(buf));
	printf("retval=%d\n", retval);
	retval = write(fd, buf, sizeof(buf));
	printf("retval=%d\n", retval);
	retval = write(fd, "neki tekst", 11);
	printf("retval=%d\n", retval);
	retval = close(fd);
	printf("closing retval=%d\n", retval);

	fd = open("/simplefs/test", O_RDONLY, 0);
	printf("fd=%d\n", fd);

	char readBuf[500];
	memset(readBuf, 'a', sizeof(readBuf));
	buf[sizeof(readBuf) - 1] = '\0';
	do {
		retval = read(fd, readBuf, sizeof(buf));
		if(retval == 0) break;

		printf("retval=%d\n", retval);
		if(retval == sizeof(buf)){
			if(strcmp(readBuf, buf) == 0){
				printf("fodder matches\n");
			}else{
				printf("fodder mismatch!\n");
			}
		}else{
			printf("buff=%s\n", readBuf);
		}
	}while(retval > 0);
	printf("\n");
}

void twoFSTest(){
	printf("----twoFSTest-----\n");
	//create, write content and copy to a new file in /disk filesystem (/simplefs and /disk)
	int fd = open("/simplefs/twoFSTest", O_CREAT | O_WRONLY, 0);
	printf("fd=%d\n", fd);
	int retval = write(fd, "neki tekst", 11);
	printf("retval=%d\n", retval);
	retval = close(fd);
	printf("retval=%d\n", retval);

	fd = open("/simplefs/twoFSTest", O_RDONLY, 0);
	printf("fd=%d\n", fd);
	char buff[11];
	retval = read(fd, buff, 11);
	printf("retval=%d\n", retval);
	printf("buff=%s\n", buff);

	fd = open("/disk/twoFSTest", O_CREAT | O_WRONLY, 0);
	printf("fd=%d\n", fd);
	retval = write(fd, buff, 11);
	printf("retval=%d\n", retval);
	retval = close(fd);
	printf("retval=%d\n", retval);

	fd = open("/disk/twoFSTest", O_RDONLY, 0);
	printf("fd=%d\n", fd);
	retval = read(fd, buff, 11);
	printf("retval=%d\n", retval);
	printf("buff=%s\n", buff);
	retval = close(fd);
	printf("retval=%d\n", retval);

	printf("\n");
}

int fs_test(char* args[]){
	printf("Example program: [%s:%s]\n%s\n\n", __FILE__, __FUNCTION__,
		   PROG_HELP);

	createWriteTwoRead();
	rewriteNoNewBlock();
	rewriteNewBlocks();
	twoFSTest();

	return 0;
}
