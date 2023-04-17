#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include<fcntl.h>

#define NUM_DATA_BLOCK 2
#define MSG_SIZE 4096
#define _GNU_SOURCE

void get_data(char *msg, size_t size, uint64_t offset)
{
    int ret;

    ret = syscall(156,offset, msg, size);

    printf("Valore di ritorno della system call - %d\n", ret);

    printf("Messaggio letto - %s\n", msg);

    memset(msg, 0, 4000);
}

void put_data(const char *msg)
{

    int ret;

    ret = syscall(174, msg, strlen(msg) + 1);

    printf("Valore di ritorno della system call - %d\n", ret);
}

void read_file()
{
    char msg_read[4000];
    int fd;
    int ret;

    memset(msg_read, 0, 4000);

    fd = open("/home/cap/Scrivania/progetto-soa/privato/progetto-soa-privato/file-system/mount/the-file", O_RDWR);

    if(fd == -1)
    {
        printf("Errore apertura del file\n");
        perror("Errore:");
        return;
    }

    printf("fd = %d\n", fd);

    ret = read(fd, (void *)msg_read, 4000);

    if(ret==-1)
    {
        perror("Errore:");
        return;
    }

    printf("Valore restituito: %d\n", ret);

    printf("Messaggi letti:\n%s", msg_read);

    close(fd);
}

int main(int argc, char** argv){
	int fd;
	char * filename, *buffer;
	ssize_t ret;
	int n;
    const char * str1 = "Nuovo messaggio 1";
    const char * str2 = "Nuovo messaggio 2";
    const char * str3 = "Nuovo messaggio 3";

/*
    char *msg;

    msg = (char *)malloc(4000);

    memset(msg, 0, 4000);


    get_data(msg, 4000, 0);

    get_data(msg, 4000, 64);

    get_data(msg, 4000, 128);

    get_data(msg, 4000, 192);
*/
/* ---------------------------------------------------------------------------------------- */

    read_file();

    put_data(str1);

    read_file();

    put_data(str2);

    read_file();

    put_data(str3);

    read_file();

/*
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

  syscall(177,2049);

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
