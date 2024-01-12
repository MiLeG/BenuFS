/*! Hello world program */

#include <stdio.h>

char PROG_HELP[] = "Filesystem demo test.";

int fs_test(char *args[])
{
	printf("Example program: [%s:%s]\n%s\n\n", __FILE__, __FUNCTION__,
		 PROG_HELP);
	int flags = O_CREAT | O_WRONLY;
	printf("flags: %d\n", flags);
	int fd = open("file:test", O_CREAT | O_WRONLY, 0);
	printf("fd=%d\n", fd);
	int retval = write(fd, "neki tekst", 11);
	printf("retval=%d\n", retval);
	retval = close(fd);
	printf("retval=%d\n", retval);

	fd = open("file:test", O_RDONLY, 0);
	printf("fd=%d\n", fd);
	char buff[11];
	retval = read(fd, buff, 11);
	printf("retval=%d\n", retval);
	printf("buff=%s\n", buff);

	int fd2 = open("file:test", O_RDONLY, 0);
	printf("second fd=%d\n", fd2);
	retval = read(fd2, buff, 11);
	printf("retval=%d\n", retval);
	printf("buff=%s\n", buff);

	close(fd);
	close(fd2);



	return 0;
}
//testing, dev todo:
//rewriteat unutar istog sizea, rewriteat više da se poveća file
//kursor pomaknut pa počet rewriteat
//peekanje, deletanje, renameanje
