#include<unistd.h>
#include<sys/syscall.h>
#include<stdio.h>
#include<string.h>
#include<stdint.h>
#include<stdlib.h>


/*
 * Questa funzione consente di invocare la system call put_data().
 * Il parametro della funzione rappresenta il messaggio da inserire
 * all'interno di un blocco libero.
 */
uint64_t put_data(const char *msg)
{

    uint64_t ret;

    ret = syscall(156, msg, strlen(msg) + 1);

    printf("[PUT DATA] Valore di ritorno della system call: %ld\n", ret);

    printf("[PUT DATA] Il messaggio inserito nel blocco %ld è: %s\n", ret, msg);

    return ret;
}


int main(int argc, char **argv)
{

    char *msg;

    if(argc!=2)
    {
        printf("./test-put messaggio\n");

        exit(1);    
    }

    msg = argv[1];

    printf("Messaggio: %s\n", msg);
    
    put_data(msg);

    return 0;
}
