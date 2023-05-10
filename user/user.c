#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include<fcntl.h>

#define MSG_SIZE 4096
#define NUM_BLOCK_DATA 100
#define _GNU_SOURCE


void get_data(uint64_t offset)
{
    int ret;

    char msg[MSG_SIZE];

    memset(msg, 0, MSG_SIZE);

    ret = syscall(156, offset, msg, MSG_SIZE);

    printf("Valore di ritorno della system call GET_DATA: %d\n"\
            "Messaggio letto: %s\n", ret, msg);

}



void put_data(const char *msg)
{

    int ret;

    ret = syscall(174, msg, strlen(msg) + 1);

    printf("Valore di ritorno della system call PUT_DATA: %d\n", ret);

}



void invalidate_data(uint64_t offset)
{
    int ret;

    ret = syscall(177,offset);

    printf("Valore di ritorno della system call INVALIDATE_DATA: %d\n", ret);
}



void read_file(char *path)
{
    char msg_read[NUM_BLOCK_DATA * MSG_SIZE];
    int fd;
    int ret;

    memset(msg_read, 0, NUM_BLOCK_DATA * MSG_SIZE);

    //fd = open("/home/cap/Scrivania/progetto-soa/privato/progetto-soa-privato/file-system/mount/the-file", O_RDWR);
    
    fd = open(path, O_RDWR);

    if(fd == -1)
    {
        printf("Errore apertura del file\n");
        perror("Errore apertura:");
        return;
    }

    ret = read(fd, (void *)msg_read, 4000);

    if(ret==-1)
    {
        printf("Errore lettura del file\n");
        perror("Errore lettura:");
        return;
    }

    printf("Valore di ritorno della operazione di READ: %d\n", ret);

    printf("Messaggi letti:\n%s", msg_read);

    close(fd);
}



int main(int argc, char** argv){
	int fd;
	char * msg;
    char *path_file;
    int sys_num;
    int offset;


    if(argc!=3)
    {
        printf( "1. Se si vuole eseguire una GET_DATA si esegua il seguente comando:\n"\
                "\t./user 0 offset-blocco\n\n"\
                "2. Se si vuole eseguire una PUT_DATA si esegua il seguente comando:\n"\
                "\t./user 1 messaggio-da-scrivere-nel-blocco\n\n"\
                "3. Se si vuole eseguire una INVALIDATE_DATA si esegua il seguente comando:\n"\
                "\t./user 2 offset-blocco\n\n"\
                "4. Se si vuole eseguire una lettura del file 'the-file' si esegua il seguente comando:\n"\
                "\t./user 3 path-file\n");
        return -1;
    }

    sys_num = atoi(argv[1]);

    switch(sys_num)
    {
        case 0:
            offset = atoi(argv[2]);
            get_data(offset);
            break;

        case 1:
            msg = argv[2];
            put_data(msg);
            break;

        case 2:
            offset = atoi(argv[2]);
            invalidate_data(offset);
            break;

        case 3:
            path_file = argv[2];
            read_file(path_file);
            break;

        default:
            printf("Il numero della system call specificato non è valido\n");
            return -1;
    }

	return 0;
}
