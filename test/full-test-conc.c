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

#define NTHREADS 2

#define ITER 5000

#define MSG_SIZE 4096




/*
 * Questa variabile consente di sincronizzare la partezza
 * dei threads in modo da farli partire insieme per tentare
 * di incrementare la concorrenza.
 */
uint64_t count = 0;
int read_err = 0;
int inval_err = 0;
int insert_err = 0;
int inval_ok = 0;
int insert_ok = 0;
int read_ok = 0;
int read_file_err = 0;
int read_file_ok = 0;



/*
 * Questa funzione consente di invocare la system call get_data().
 * Il parametro della funzione rappresenta l'indice del blocco che si
 * vuole leggere.
 */
int get_data(uint64_t offset)
{
    int ret;

    char msg[MSG_SIZE];

    memset(msg, 0, MSG_SIZE);

    ret = syscall(156, offset, msg, MSG_SIZE);
    
    return ret;

}


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
 * Questa funzione consente di invocare la system call put_data().
 * Il parametro della funzione rappresenta il messaggio da inserire
 * all'interno di un blocco libero.
 */
uint64_t put_data(const char *msg)
{

    uint64_t ret;

    ret = syscall(174, msg, strlen(msg));

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

    while(count!=4*NTHREADS);

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
    }

    printf("Il thread inseritore %ld ha terminato\n", id_thread);

    fflush(stdout);
    
}



void * read_block(void *id)
{

    uint64_t id_thread = (uint64_t)id;
    uint64_t index = id_thread;
    int ret;

    __sync_fetch_and_add(&count,1);

    while(count!=4*NTHREADS);

    printf("[READ] Il thread %ld inizia le sue esecuzioni....\n", id_thread);

    for(int i = 0; i < ITER; i++)
    {

        if(i%10==0)
        {
            printf("Il thread lettore %ld si trova a %d\n", id_thread, i);    
        }


        index = ((index * 3) + 93) % NBLOCKS;

        ret = get_data(index);

        if(ret == -1)
        {
            __sync_fetch_and_add(&read_err,1);
        }
        else
        {
            __sync_fetch_and_add(&read_ok,1);
        }

    }

    printf("Il thread lettore %ld ha terminato\n", id_thread);

    fflush(stdout);
}



void *read_the_file(void *id)
{
    uint64_t id_thread = (uint64_t)id;
    uint64_t index = id_thread;
    int ret;
    char msg_read[10000];
    int fd;

    __sync_fetch_and_add(&count,1);

    while(count!=4*NTHREADS);

    printf("[THE-FILE] Il thread %ld inizia le sue esecuzioni....\n", id_thread);

    for(int i = 0; i < ITER; i++)
    {

        if(i%10==0)
        {
            printf("Il thread THE-FILE %ld si trova a %d\n", id_thread, i);    
        }

        memset(msg_read, 0, 10000);

        fd = open("/home/cap/Scrivania/progetto-soa/privato/progetto-soa-privato/file-system/mount/the-file", O_RDWR);

        if(fd == -1)
        {
            __sync_fetch_and_add(&read_file_err,1);
            close(fd);
            continue;
        }

        ret = read(fd, (void *)msg_read, 10000);

        if(ret==-1)
        {
            __sync_fetch_and_add(&read_file_err,1);
            close(fd);
            continue;
        }

        __sync_fetch_and_add(&read_file_ok,1);

        close(fd);        
        
    }

    printf("Il thread THE-FILE %ld ha terminato\n", id_thread);

    fflush(stdout);
}




void * inval_block_with_thread(void *id)
{
    uint64_t id_thread = (uint64_t)id;
    uint64_t index = id_thread;
    int ret;

    __sync_fetch_and_add(&count,1);

    while(count!=4*NTHREADS);

    printf("[INVAL] Il thread %ld inizia le sue esecuzioni....\n", id_thread);

    for(int i = 0; i < ITER; i++)
    {

        if(i%10==0)
        {
            printf("Il thread invalidatore %ld si trova a %d\n", id_thread, i);    
        }


        index = ((index * 21) + 9) % NBLOCKS;
        ret = invalidate_data(index);

        if(ret == -1)
        {
            __sync_fetch_and_add(&inval_err,1);
        }
        else
        {
            __sync_fetch_and_add(&inval_ok,1);
        }

    }

    printf("Il thread invalidatore %ld ha terminato\n", id_thread);

    fflush(stdout);
    
}



int main(void)
{
    uint64_t i;
    pthread_t tid_insert[NTHREADS];
    pthread_t tid_inval[NTHREADS];
    pthread_t tid_read[NTHREADS];
    pthread_t tid_file[NTHREADS];


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

    /* Creazione dei thread per le letture */

    for(i=0;i<NTHREADS; i++)
    {
        pthread_create(&tid_read[i],NULL,read_block,(void *)(i));
    }

    printf("Lettori creati con successo\n");

    /* Creazione dei thread per le letture da file */

    for(i=0;i<NTHREADS; i++)
    {
        pthread_create(&tid_file[i],NULL,read_the_file,(void *)(i));
    }

    printf("Lettori the file creati con successo\n");

    /* Attendo la terminazione dei thread per gli inserimenti */


    for(i=0;i<NTHREADS; i++)
    {
        pthread_join(tid_insert[i], NULL);
    }

    printf("Inserimenti completati\n");

    /* Attendo la terminazione dei thread per le letture  */

    for(i=0;i<NTHREADS; i++)
    {
        pthread_join(tid_read[i], NULL);
    }

    printf("Letture completate\n");

    /* Attendo la terminazione dei thread per le invalidazioni */

    for(i=0;i<NTHREADS; i++)
    {
        pthread_join(tid_inval[i], NULL);
    }

    for(i=0;i<NTHREADS; i++)
    {
        pthread_join(tid_file[i], NULL);
    }

    printf("invalidazioni completate\n");

    printf("Esecuzione completata.\n");

    printf("Errori inserimenti: %d\n", insert_err);

    printf("Corretti inserimenti: %d\n", insert_ok);

    printf("Errori invalidazioni: %d\n", inval_err);

    printf("Corrette invalidazioni: %d\n", inval_ok);

    printf("Errori letture: %d\n", read_err);

    printf("Corrette letture: %d\n", read_ok);

    printf("Errori letture the file: %d\n", read_file_err);

    printf("Corrette letture the file: %d\n", read_file_ok);

    return 0;

}





















