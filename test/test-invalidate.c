#include<unistd.h>
#include<sys/syscall.h>
#include<stdio.h>
#include<string.h>
#include<stdint.h>
#include<stdlib.h>



/*
 * Questa funzione consente di invocare la system call invalidate_data().
 * Il parametro della funzione rappresenta l'indice del blocco che si
 * vuole invalidare.
 */
int invalidate_data(uint64_t offset)
{
    int ret;

    ret = syscall(174,offset);

    return ret;
}



int main(int argc, char **argv)
{
    int ret;
    uint64_t offset;

    if(argc!=2)
    {
        printf("./test-invalidate offset\n");
        exit(1);
    }

    offset = atoll(argv[1]);

    ret=invalidate_data(offset);

    if(ret!=-1)
    {
        printf("[INVALIDATE] Il blocco %ld è stato invalidato con successo\n", offset);
    }
    else
    {
        printf("[INVALIDATE] Il blocco %ld NON è stato invalidato con successo\n", offset);
    }

    return 0;
}
