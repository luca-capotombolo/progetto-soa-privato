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



/*
 * Questa funzione consente di invocare la system call get_data().
 * I parametri della funzione rappresentano:
 * - msg: Il buffer di memoria dove inserire il messaggio del blocco.
 * - size: Il numero di byte da leggere.
 * - offset: L'indice del blocco di cui si vuole leggere il messaggio.
 */
int get_data(char *msg, size_t size, uint64_t offset)
{
    int ret;

    ret = syscall(134,offset, msg, size);

    return ret;

}



int main(int argc, char **argv)
{

    int ret;
    int size;
    uint64_t offset;
    char msg[10000];


    if(argc!=3)
    {
        printf("./test-get offset size\n");
        exit(1);
    }

    offset = atoll(argv[1]);

    size = atoll(argv[2]);

    if(size > 10000)
    {
        printf("Size deve essere minore di 10000\n");
        return -1;
    }

    memset(msg,0,10000);

    ret=get_data(msg, size, offset);

    if(ret!=-1)
    {
        printf("Numero di byte restituiti: %d\n", ret);
        printf("%s\n", msg);
    }
    else
    {
        printf("Errore lettura blocco %ld\n", offset);
    }

    return 0;

}
