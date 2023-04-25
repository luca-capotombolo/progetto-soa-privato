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
#define NBLOCKS 10
#define DIM_BUFFER 4096 * NBLOCKS



/*
 * Questa variabile consente di sincronizzare la partezza
 * dei threads in modo da farli partire insieme per tentare
 * di incrementare la concorrenza.
 */
int count = 0;

int invalidate_test_first_time[NBLOCKS];

int write_test_first_time[NBLOCKS];



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

    printf("[GET DATA] Valore di ritorno della system call - %d\n", ret);

    printf("[GET DATA] Messaggio letto dal blocco %ld: %s\n", offset, msg);

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

    printf("[PUT DATA] Valore di ritorno della system call: %ld\n", ret);
    printf("[PUT DATA] Il messaggio inserito nel blocco %ld è: %s\n", ret, msg);
}



/*
 * Questa funzione ha il compito di leggere il contenuto del
 * file contenente i messaggi in ordine di inserimento.
 */
void read_file()
{
    char msg_read[DIM_BUFFER];
    int fd;
    int ret;

    memset(msg_read, 0, DIM_BUFFER);

    fd = open("/home/cap/Scrivania/progetto-soa/privato/progetto-soa-privato/file-system/mount/the-file", O_RDWR);

    if(fd == -1)
    {
        perror("[READ FILE]  Errore apertura del file:");
        return;
    }

    printf("[READ FILE] Il file descriptor associato al file è %d\n", fd);

    ret = read(fd, (void *)msg_read, DIM_BUFFER);

    if(ret==-1)
    {
        perror("[READ FILE] Errore lettura del file:");
        return;
    }

    printf("[READ FILE] I messaggi che sono stati letti dal device sono:\n%s", msg_read);

    close(fd);
}



/*
 * Questa funzione viene eseguita da un thread in un blocco di
 * di thread per leggere uno specifico blocco.
 */
void * read_single_block_with_thread(void *arg)
{
    char *msg;
    uint64_t block;

    block = (uint64_t)arg;

    msg = (char *)malloc(4096);

    memset(msg, 0, 4096);

    __sync_fetch_and_add(&count,1);

    while(count!=NBLOCKS);

    get_data(msg, 4096, block);

    printf(" [READ SINGLE THREAD WITH THREAD] Il messaggio letto dal blocco %ld è %s\n", block, msg);

    free(msg);    
}



/*
 * Questa funzione ha il computo di generare tanti threads
 * quanti sono i blocchi per poter leggere il loro contenuto.
 */ 
void read_all_block_with_threads(void)
{
    uint64_t i;
    pthread_t tid[NBLOCKS];

    if(NBLOCKS > 300)
    {
        printf("[ERRORE READ ALL BLOCKS WITH THREAD] Ci sono troppi blocchi per creare i threads\n");
        return;
    }

    for(i=0; i<NBLOCKS; i++)
    {
        pthread_create(&tid[i],NULL,read_single_block_with_thread,(void *)i);
    }

    for(i=0; i<NBLOCKS; i++)
    {
        pthread_join(tid[i], NULL);
    }

    count = 0;

    printf("[READ ALL BLOCKS WITH THREAD] La lettura dei blocchi è completata con successo\n");
}



/*
 * Questa funzione viene eseguita da un thread tra
 * un gruppo di thread e ha lo scopo di scrivere
 * l'identificativo del thread all'interno di un
 * blocco.
 */
void put_data_thread(uint64_t id)
{
    char msg[80];
    uint64_t ret;

    memset(msg,0,80);

    sprintf(msg, "%ld", id);

    __sync_fetch_and_add(&count,1);

    while(count!=NBLOCKS);

    ret = syscall(174, msg, strlen(msg) + 1);

    write_test_first_time[id] = ret;

    printf("[PUT DATA THREAD] Messaggio scritto nel blocco %ld: %s\n", ret, msg);    
}



/*
 * Questa funzione ha il compito di scrivere su un blocco
 * del dispositivo. Viene scritto l'identificativo del thread
 * che esegue la funzione.
 */
void * write_block(void *arg)
{
    put_data_thread((uint64_t)arg);    
}



/*
 * Questa funzione ha il compito di generare tanti
 * thread quanti sono i blocchi per scrivere un messaggio.
 */
void write_all_blocks_with_threads(void)
{
    uint64_t i;
    pthread_t tid[NBLOCKS];

    if(NBLOCKS > 300)
    {
        printf("[ERRORE WRITE ALL BLOCKS WITH THREADS] Ci sono troppi blocchi per creare i threads\n");
        return;
    }

    for(i=0;i<NBLOCKS; i++)
    {
        pthread_create(&tid[i],NULL,write_block,(void *)(i));
    }

    for(i=0; i<NBLOCKS; i++)
    {
        pthread_join(tid[i], NULL);
    }

    count = 0;

    printf("[WRITE ALL BLOCKS WITH THREADS] Le scritture sono state eseguite su tutti i blocchi\n");
}



/*
 * Questa funzione viene eseguita da un thread in un gruppo
 * di thread. Ha il compito di eseguire l'invalidazione del
 * blocco il cui ID è passato come parametro.
 */
void * invalidate_block_with_thread(void *id)
{
    int ret;

    uint64_t index = (uint64_t)id;

    __sync_fetch_and_add(&count,1);

    while(count!=NBLOCKS);

    ret = invalidate_data((uint64_t)id);

    if(ret == -1)
    {
        printf("[ERRORE INVALIDATE BLOCK WITH THREAD] L'invalidazione per il blocco %ld è terminata con un insuccesso\n", (uint64_t)id);
    }
    else
    {
        printf("[INVALIDATE BLOCK WITH THREAD] L'invalidazione per il blocco %ld è terminata con un successo\n", (uint64_t)id);
    }

    invalidate_test_first_time[index] = ret;

    
}


/*
 * Questa funzione ha lo scopo di invalidare il contenuto
 * di tutti i blocchi del dispositivo. Per fare ciò, crea
 * un numero di thread che è pari al numero di blocchi.
 */
void invalidate_all_blocks_with_threads(void)
{
    uint64_t i;
    pthread_t tid[NBLOCKS];

    if(NBLOCKS > 300)
    {
        printf("[ERRORE INVALIDATE ALL BLOCKS WITH THREAD] Ci sono troppi blocchi per creare i threads\n");
        return;
    }

    for(i=0;i<NBLOCKS; i++)
    {
        pthread_create(&tid[i],NULL,invalidate_block_with_thread,(void *)(i));
    }

    for(i=0; i<NBLOCKS; i++)
    {
        pthread_join(tid[i], NULL);
    }

    count = 0;

    printf("[INVALIDATE ALL BLOCKS WITH THREAD] Le invalidazioni sono state eseguite su tutti i blocchi\n");
}



int main(int argc, char** argv){
    int i;
	
    invalidate_all_blocks_with_threads();

    for(i=0; i<NBLOCKS; i++)
    {
        if(invalidate_test_first_time[i] != -1)
        {
            printf("Valore i = %d\n", i);
            printf("Valore %d\n", invalidate_test_first_time[i]);
            printf("Errore invalidate_test_first_time\n");
            return 1;
        }
    }

    printf("Test #0 passato con successo\n");

    write_all_blocks_with_threads();

    for(i=0; i<NBLOCKS; i++)
    {
        if(write_test_first_time[i] == -1)
        {
            printf("Valore i = %d\n", i);
            printf("Valore %d\n", write_test_first_time[i]);
            printf("Errore write_test_first_time\n");
            return 1;
        }
    }

    printf("Test #1 passatto con successo\n");

	return 0;
}
