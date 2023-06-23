#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>

#define _GNU_SOURCE

#define NBLOCKS 50000

#define NTHREADS 4

#define ITER 50

#define MSG_SIZE 4096

int insert_ok = 0;
int insert_err = 0;
uint64_t count = 0;

/*
 * Questa funzione consente di invocare la system call put_data().
 * Il parametro della funzione rappresenta il messaggio da inserire
 * all'interno di un blocco libero.
 */
uint64_t put_data(const char *msg)
{

    uint64_t ret;

    ret = syscall(156, msg, strlen(msg));

    return ret;
}


void * insert_block_with_thread(void *id)
{
    uint64_t id_thread = (uint64_t)id;
    uint64_t index = id_thread;
    char buff[10];
    int ret;

    memset(buff,0,10);

    sprintf(buff, "%ld", id_thread);

    __sync_fetch_and_add(&count,1);

    while(count!=NTHREADS);

    printf("[INSERT] Il thread %ld inizia le sue esecuzioni....\n", id_thread);

    for(int i = 0; i < ITER; i++)
    {
        if(i%10==0)
        {
            printf("Il thread inseritore %ld si trova a %d\n", id_thread, i);    
        }

        ret = put_data(buff);

        if(ret == -1)
        {
            __sync_fetch_and_add(&insert_err,1);
            printf("[ERRORE PUT DATA]\n");
        }
        else
        {
            __sync_fetch_and_add(&insert_ok,1);
        }

        index = ((index * 6) + 21) % NBLOCKS;

        //usleep((index % 15) * 100000);
    }

    printf("Il thread inseritore %ld ha terminato\n", id_thread);

    fflush(stdout);
    
}


int main(void)
{
    uint64_t i;
    pthread_t tid_insert[NTHREADS];

    /* Creazione dei thread per gli inserimenti */

    for(i=0;i< NTHREADS; i++)
    {
        pthread_create(&tid_insert[i],NULL,insert_block_with_thread,(void *)(i));
    }

    printf("Scrittori creati con successo\n");

    /* Attendo la terminazione dei thread per gli inserimenti */


    for(i=0;i<NTHREADS; i++)
    {
        pthread_join(tid_insert[i], NULL);
    }

    printf("Esecuzione completata.\n");

    printf("Errori inserimenti: %d\n", insert_err);

    printf("Corretti inserimenti: %d\n", insert_ok);

    return 0;

}
