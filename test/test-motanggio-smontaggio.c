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

    return ret;
}


int main(void)
{
    char *msg;
    uint64_t offset;

    msg = (char *)malloc(4096);

    memset(msg,0,4096);

    get_data(msg, 4096, 0);

    offset = put_data("Prima scrittura.");

    memset(msg, 0, 4096);

    get_data(msg, 4096, offset);
/*
    invalidate_data(2);
    invalidate_data(3);
    invalidate_data(4);
    invalidate_data(5);
    invalidate_data(7);
    invalidate_data(3);
    invalidate_data(9);
    invalidate_data(13);
    invalidate_data(15);
    invalidate_data(16);
*/
    return 0;

    
}












