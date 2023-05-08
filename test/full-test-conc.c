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

#define NBLOCKS 50

#define NTHREADS 10

#define ITER 200




/*
 * Questa variabile consente di sincronizzare la partezza
 * dei threads in modo da farli partire insieme per tentare
 * di incrementare la concorrenza.
 */
uint64_t count_insert = 0;
uint64_t count_inval = 0;
int inval_err = 0;
int insert_err = 0;
int inval_ok = 0;
int insert_ok = 0;



/*
 * Questa funzione consente di invocare la system call invalidate_data().
 * Il parametro della funzione rappresenta l'indice del blocco che si
 * vuole invalidare.
 */
int invalidate_data(uint64_t offset)
{
    int ret;

    ret = syscall(177,offset);

    return ret;
}



/*
 * Questa funzione consente di invocare la system call get_data().
 * I parametri della funzione rappresentano:
 * - msg: Il buffer di memoria dove inserire il messaggio del blocco.
 * - size: Il numero di byte da leggere.
 * - offset: L'indice del blocco di cui si vuole leggere il messaggio.
 */
void get_data(char *msg, size_t size, uint64_t offset)
{
    int ret;

    ret = syscall(156,offset, msg, size);

}



/*
 * Questa funzione consente di invocare la system call put_data().
 * Il parametro della funzione rappresenta il messaggio da inserire
 * all'interno di un blocco libero.
 */
uint64_t put_data(const char *msg)
{

    uint64_t ret;

    ret = syscall(174, msg, strlen(msg) + 1);

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

    __sync_fetch_and_add(&count_insert,1);

    while(count_insert!=NTHREADS);

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

        index = ((index * 2) + 7) % NBLOCKS;

        usleep((index % 3) * 100000);
    }

    printf("Il thread inseritore %ld ha terminato\n", id_thread);
    
}





void * inval_block_with_thread(void *id)
{
    uint64_t id_thread = (uint64_t)id;
    uint64_t index = id_thread;
    int ret;

    __sync_fetch_and_add(&count_inval,1);

    while(count_inval!=NTHREADS);

    printf("[INVAL] Il thread %ld inizia le sue esecuzioni....\n", id_thread);

    for(int i = 0; i < ITER; i++)
    {

        if(i%10==0)
        {
            printf("Il thread invalidatore %ld si trova a %d\n", id_thread, i);    
        }


        index = ((index * 2) + 7) % NBLOCKS;
        ret = invalidate_data(index);

        if(ret == -1)
        {
            __sync_fetch_and_add(&inval_err,1);
        }
        else
        {
            __sync_fetch_and_add(&inval_ok,1);
        }

        usleep((index % 12) * 100000);

    }

    printf("Il thread invalidatore %ld ha terminato\n", id_thread);
    
}



int main(void)
{
    uint64_t i;
    pthread_t tid_insert[NTHREADS];
    pthread_t tid_inval[NTHREADS];


    /* Creazione dei thread per gli inserimenti */

    for(i=0;i< NTHREADS; i++)
    {
        pthread_create(&tid_insert[i],NULL,insert_block_with_thread,(void *)(i));
    }

    printf("Scrittori creati con successo\n");

    /* Creazione dei thread per le invalidazioni */

    for(i=0;i<NTHREADS; i++)
    {
        pthread_create(&tid_inval[i],NULL,inval_block_with_thread,(void *)(i));
    }

    printf("Invalidatori creati con successo\n");

    /* Attendo la terminazione dei thread per gli inserimenti */

    for(i=0;i<NTHREADS; i++)
    {
        pthread_join(tid_insert[i], NULL);
    }

    /* Attendo la terminazione dei thread per le invalidazioni */

    for(i=0;i<NTHREADS; i++)
    {
        pthread_join(tid_inval[i], NULL);
    }

    printf("Esecuzione completata.\n");

    printf("Errori inserimenti: %d\n", insert_err);

    printf("Corretti inserimenti: %d\n", insert_ok);

    printf("Errori invalidazioni: %d\n", inval_err);

    printf("Corrette invalidazioni: %d\n", inval_ok);

    return 0;

}





















