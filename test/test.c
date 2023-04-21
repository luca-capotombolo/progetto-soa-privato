#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <fcntl.h>
#include <pthread.h>

#define _GNU_SOURCE
#define NBLOCKS 8

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



void * read_block(void *arg)
{
    char *msg;
    uint64_t block;

    block = (uint64_t)arg;

    printf("Richiesta lettura per il blocco %ld\n", block);

    msg = (char *)malloc(4000);

    memset(msg, 0, 4000);

    get_data(msg, 4000, block);

    free(msg);    
}

void empty_device(void)
{
    uint64_t i;
    pthread_t tid;

    for(i=0; i<NBLOCKS; i++)
    {
        pthread_create(&tid,NULL,read_block,(void *)i);
    }
}


void * write_block(void *arg)
{
    put_data("Ciao a tutti\n");    
}



void write_all_blocks(void)
{
    uint64_t i;
    pthread_t tid;

    for(i=0;i<NBLOCKS; i++)
    {
        pthread_create(&tid,NULL,write_block,(void *)i);
    }
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

    read_file();

    put_data(str1);

    read_file();

    put_data(str2);

    read_file();

    put_data(str3);

    read_file();

    msg = (char *)malloc(4000);

    memset(msg, 0, 4000);

    get_data(msg, 4000, 0);

    get_data(msg, 4000, 64);

    get_data(msg, 4000, 128);

    get_data(msg, 4000, 192);
*/

/* ---------------------------------------------------------------------------------------- */

    //empty_device();

    //put_data(str1);

    //empty_device();

    //read_file();

    write_all_blocks();

    pause();


	return 0;
}
