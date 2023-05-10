#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <stdint.h>
#include <fcntl.h>

#define NTHREADS 100
#define ITER 10
#define NUM_BLOCK_DATA 50
#define MSG_SIZE 4096


int count = 0;
int num_err = 0;
char msg[8] = "A\nB\nC\nD";

void *read_file(void *arg)
{
    char msg_read[NUM_BLOCK_DATA * MSG_SIZE];
    int fd;
    int ret;

    __sync_fetch_and_add(&count,1);

    while(count!=NTHREADS);

    memset(msg_read, 0, NUM_BLOCK_DATA * MSG_SIZE);

    for(int i=0; i < ITER; i++)
    {

        fd = open("/home/cap/Scrivania/progetto-soa/privato/progetto-soa-privato/file-system/mount/the-file", O_RDWR);

        if(fd == -1)
        {
            printf("Errore apertura del file\n");
            perror("Errore apertura:");
            return NULL;
        }

        ret = read(fd, (void *)msg_read, NUM_BLOCK_DATA * MSG_SIZE);

        if(ret!=-1)
        {
            ret = strncmp(msg, msg_read, 7);

            if(ret!=0)
            {
                __sync_fetch_and_add(&num_err,1);
            }

            memset(msg_read, 0, NUM_BLOCK_DATA * MSG_SIZE); 

            close(fd);              
        }
    }
}



int main(void)
{
    uint64_t i;
    pthread_t tids[NTHREADS];

    /* Creazione dei thread per gli inserimenti */

    for(i=0;i< NTHREADS; i++)
    {
        pthread_create(&tids[i],NULL,read_file,NULL);
    }

    /* Attendo la terminazione dei thread per gli inserimenti */

    for(i=0;i<NTHREADS; i++)
    {
        pthread_join(tids[i], NULL);
    }

    printf("Esecuzione completata.\n");

    printf("Errori letture: %d\n", num_err);

    return 0;

}
