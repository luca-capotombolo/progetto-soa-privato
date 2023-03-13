#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include<fcntl.h>

#define NUM_DATA_BLOCK 2
#define MSG_SIZE 4096

int main(int argc, char** argv){
	int fd;
	char * filename, *buffer;
	ssize_t ret;
	int n;
    const char * str = "ciao come stai? Spero tutto bene!";
    const char * str2 = "Proviamo con il secondo tentativo... dovrebbe andare tutto bene.";
    const char * str3 = "Blocco #0.";
    char *msg;

    msg = (char *)malloc(4000);
    memset(msg,0,4000);

	syscall(156,0, msg, 4000);
    printf("Blocco dati #0: %s\n", msg);
    memset(msg,0,4000);

	syscall(156,1, msg, 4000);
    printf("Blocco dati #1: %s\n", msg);
    memset(msg,0,4000);

	ret = syscall(174,str, strlen(str)+1);
    printf("Byte letti: %ld\n", ret);
    memset(msg,0,4000);

	syscall(156,0, msg, 4000);
    printf("Blocco dati #0: %s\n", msg);
    memset(msg,0,4000);

	syscall(156,1, msg, 4000);
    printf("Blocco dati #1: %s\n", msg);
    memset(msg,0,4000);

	ret = syscall(174,str2, strlen(str2)+1);
    printf("Byte letti: %ld\n", ret);
    memset(msg,0,4000);

	syscall(156,0, msg, 4000);
    printf("Blocco dati #0: %s\n", msg);
    memset(msg,0,4000);

	syscall(156,1, msg, 4000);
    printf("Blocco dati #1: %s\n", msg);
    memset(msg,0,4000);

	ret = syscall(174,str, strlen(str)+1);
    printf("Byte letti: %ld\n", ret);
    memset(msg,0,4000);

	syscall(156,0, msg, 4000);
    printf("Blocco dati #0: %s\n", msg);
    memset(msg,0,4000);

	syscall(156,1, msg, 4000);
    printf("Blocco dati #1: %s\n", msg);
    memset(msg,0,4000);

	ret = syscall(174,str3, strlen(str3)+1);
    printf("Byte letti: %ld\n", ret);
    memset(msg,0,4000);

	syscall(156,0, msg, 4000);
    printf("Blocco dati #0: %s\n", msg);
    memset(msg,0,4000);

	syscall(156,1, msg, 4000);
    printf("Blocco dati #1: %s\n", msg);
    memset(msg,0,4000);

/*  syscall(177,1);

	if(argc != 2)
	{
		printf("./user filename\n");
		return -1;
	}

	filename = argv[1];

	printf("Tentativo di apertura file %s\n", filename);

	fd = open(filename, O_RDWR);

	if(fd==-1)
	{
		printf("Errore apertura file\n");
		return -1;
	}

	n = NUM_DATA_BLOCK * MSG_SIZE;
	buffer = (char *)malloc(n);

	if(buffer==NULL)
	{
		printf("Errore allocazione malloc\n");
		return -1;
	}

	ret = read(fd, buffer, n);

	if(ret != 1)
	{
		printf("Errore nella lettura dei dati\n");
		return -1;
	}

	printf("Messaggio letto dal file:\n%s\n", buffer);
*/
	return 0;
}
