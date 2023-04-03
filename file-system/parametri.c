#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include "../headers/file_system.h"

/*
 * Il parametro che viene passato in input rappresenta
 * il numero dei blocchi di dati insieme ai due blocchi
 * contenenti rispettivamente il superblocco e l'inode.
 * Questo programma computa il numero dei blocchi di stato
 * che devono essere utilizzati per poi stampare il numero
 * TOTALE di blocchi che tiene conto:
 *      - del blocco contenente il superblocco
 *      - del blocco contenente l'inode
 *      - dei blocchi di stato
 *      - dei blocchi di dati
 */

int main(int argc, char **argv)
{

    uint64_t nblocks;
    uint64_t nblocks_state;
    uint64_t nblocks_data;

    if(argc != 2)
    {
        printf("./parametri <Numero-totale-di-blocchi\n");
        return -1;
    }

    /*
     * Recupero il numero dei blocchi del device senza contare
     * il numero dei blocchi di stato.
     */
    nblocks = atoi(argv[1]);

    if(nblocks < 3)
    {
        printf("Si necessita di almeno 3 blocchi. Numero totale di blocchi è pari a %ld\n", nblocks);
        return -1;
    }

    /* Computo il numero dei blocchi di dati.*/
    nblocks_data = nblocks - 2;

    /* Computo il numero dei blocchi di stato.
     * Il valore SOAFS_BLOCK_SIZE << 3 rappresenta
     * il numero di bit che ho a disposizione in un
     * singolo blocco. La differenza nblocks - 2
     * rappresenta il nummero di blocchi di dati
     */
    if((nblocks_data % (SOAFS_BLOCK_SIZE << 3)) == 0)
    {
        nblocks_state = (nblocks_data) / (SOAFS_BLOCK_SIZE << 3);
    }
    else{
        nblocks_state = ((nblocks_data) / (SOAFS_BLOCK_SIZE << 3)) + 1;
    }
    

    /* Assumo che anche i blocchi di stato contino nel totale */
    if( (nblocks + nblocks_state) > NBLOCKS )
    {
        printf("E' stato richiesto un numero di blocchi %ld scorretto.\nIl numero di blocchi non deve essere superiore a %d\n", nblocks + nblocks_state, NBLOCKS);
        return -1;
    }

    printf("Numero di blochi del device senza i blocchi di stato: %ld\n", nblocks);
    printf("Numero dei blocchi di dati: %ld\n", nblocks_data);
    printf("Numero di blocchi di stato: %ld\n", nblocks_state);
    printf("Numero totale di blocchi inclusi i blocchi di stato da utilizzare per il device: %ld\n", nblocks + nblocks_state);

    return 0;   
    
}
