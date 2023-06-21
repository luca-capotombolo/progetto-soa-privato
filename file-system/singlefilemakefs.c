#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "../headers/file_system.h"

/*
 * Il terzo argomento rappresenta il numero totale di blocchi
 * senza tener conto del numero dei blocchi di stato. Infatti,
 * il numero dei blocchi di stato viene computato in funzione
 * del numero totale di blocchi e può cambiare.
 */

int main(int argc, char *argv[])
{
	int fd;
    int nbytes;
    int i;
    int j;
    int n;
	char *block_padding;
	char *file_body = "Blocco #%d.";    
    ssize_t ret;   
    uint64_t nblocks;                           /* Numero totale di blocchi */    
    uint64_t nblocks_state;                     /* Numero totale blocchi di stato */    
    uint64_t nblocks_data;                      /* Numero totale dei blocchi di dati */    
    uint64_t actual_size;                       /* Dimensione effettiva dell'array dei blocchi liberi */    
    uint64_t update_list_size;                  /* Numero massimo di blocchi da caricare nella lista ad ogni aggiornamento */    
    uint64_t *block_state;                      /* Maschera di bit */
	struct soafs_super_block sb;
	struct soafs_inode file_inode;
    struct soafs_block *block = NULL;

	if (argc != 5) {
		printf("./singlefilemakefs <device> <Numero-totale-di-blocchi-senza-blocchi-di-stato> <update_list_size> <actual-size>\n");
		return -1;
	}

    /* Recupero il device da aprire */
	fd = open(argv[1], O_RDWR);

	if (fd == -1) {
		perror("Errore nell'apertura del device:");
		return -1;
	}

    update_list_size = atoi(argv[3]);

    actual_size = atoi(argv[4]);

    if(SIZE_INIT > 505)
    {
        printf("Il valore di SIZE_INIT non può essere maggiore di 505\n");
        return -1;
    }

    /*
     * Verifico se si sta chiedendo di caricare nella lista dei blocchi liberi un numero
     * di blocchi che è maggiore della dimensione massima della lista.
     */

    if(actual_size > SIZE_INIT)
    {
        printf("La dimensione richiesta %ld è maggiore della dimensione massima dell'array %d\n", actual_size, SIZE_INIT);
        printf("Il valore di actual_size > SIZE_INIT\n");
        return -1;
    }

    /*
     * Il valore di update_list_size verrà utilizzato per
     * determinare la dimensione dell'array al successivo
     * montaggio.
     */
    if(update_list_size > SIZE_INIT)
    {
        printf("La dimensione richiesta %ld è maggiore della dimensione massima dell'array %d\n", update_list_size, SIZE_INIT);
        printf("Il valore di update_list_size > SIZE_INIT\n");
        return -1;
    }

    /*
     * Recupero il numero di blocchi del device senza contare
     * il numero di blocchi di stato.
     */
    nblocks = atoi(argv[2]);

    if(nblocks < 3)
    {
        printf("Si necessita di almeno 3 blocchi in modo da avere un blocco di dati.\nNumero totale di blocchi è pari a %ld\n", nblocks);
        return -1;
    }

    /* Computo il numero dei blocchi di dati. */
    nblocks_data = nblocks - 2;

    if(SIZE_INIT > nblocks_data)
    {
        printf("Il valore di SIZE_INIT è maggiore del numero dei blocchi di dati\n");
        return -1;
    }

    if(actual_size > nblocks_data)
    {
        printf("Si sta richiedendo un array con una dimensione maggiore del numero massimo dei blocchi di dati\n");
        printf("Il valore di actual_size > nblocks\n");
        return -1;
    }

    if(update_list_size > nblocks_data)
    {
        printf("Il numero massimo di blocchi da recuperare dal device è maggiore del numero massimo dei blocchi di dati\n");
        printf("Il valore di update_list_size > nblocks\n");
        return -1;
    }

    /* Computo il numero dei blocchi di stato. Il valore SOAFS_BLOCK_SIZE << 3 rappresenta
     * il numero di bit che ho a disposizione in un singolo blocco. La differenza nblocks - 2
     * rappresenta il nummero dei blocchi di dati.
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
        printf("E' stato richiesto un numero totale di blocchi pari a %ld.\n", nblocks + nblocks_state);
        printf("Il numero di blocchi non deve essere superiore a %d\n", NBLOCKS);
        return -1;
    }
    
    printf("Il nome del device è: %s\n", argv[1]);
    printf("La dimensione massima dell'array è pari a %d\n", SIZE_INIT);
    printf("La dimensione dell'aggiornamento della lista dei blocchi liberi è pari a %ld\n", update_list_size);
    printf("La dimensione effettiva dell'array è pari a %ld\n", actual_size);
    printf("Numero di blochi del device: %ld\n", nblocks);
    printf("Numero di blocchi di stato: %ld\n", nblocks_state);
    printf("Numero totale di blocchi inclusi i blocchi di stato: %ld\n", nblocks + nblocks_state);
    fflush(stdout);


    /* Magic Number del File System */
	sb.magic = SOAFS_MAGIC_NUMBER;

    /* Numero di blocchi */
    sb.num_block = nblocks + nblocks_state;

    /* Numero dei blocchi liberi */
    sb.num_block_free = nblocks_data;

    /* Numero dei blocchi di stato */
    sb.num_block_state = nblocks_state;

    /* Numero massimo di blocchi al caricamento */
    sb.update_list_size = update_list_size;

    /*
     * inserisco i primi actual_size blocchi liberi poiché in questa versione assumo che tutti i blocchi sono
     * inizialmente liberi.
     */
    for(uint64_t k=0; k<actual_size; k++)
    {
        sb.index_free[k] = k;
    }

    /* Dimensione effettiva dell'array */
    sb.actual_size = actual_size;

    /*
     * Inizialmente la Sorted List logica è vuota. Per indicare che il valore della testa è NULL
     * ho deciso di utilizzare un indice che non appartiene a nessun blocco di dati.    
     */

    sb.head_sorted_list = sb.num_block;

	ret = write(fd, (char *)&sb, sizeof(sb));

	if (ret != SOAFS_BLOCK_SIZE)
    {
		printf("Il numero di bytes che sono stati scritti [%d] non è uguale alla dimensione del blocco.\n", (int)ret);
		close(fd);
		return -1;
	}

	printf("Il superblocco è stato scritto con successo.\n");
    fflush(stdout);

	/* Popolo l'inode del file */

    /*  Mode */
	file_inode.mode = S_IFREG;

    /* Identificativo inode del file */
	file_inode.inode_no = SOAFS_FILE_INODE_NUMBER;

    /* Numero dei blocchi di dati */
    file_inode.data_block_number = nblocks_data;

	file_inode.file_size = 0;

    /* Scrittura dei dati effettivi dell'inode */
	ret = write(fd, (char *)&file_inode, sizeof(file_inode));

	if (ret != sizeof(file_inode))
    {
		printf("Errore nella scrittura dei dati effettivi del blocco che mantiene l'inode del file.\n");
		close(fd);
		return -1;
	}

	
	/* Padding per il blocco contenente l'inode */
	nbytes = SOAFS_BLOCK_SIZE - sizeof(file_inode);

	block_padding = malloc(nbytes);

    if(block_padding == NULL)
    {
        printf("Errore nell'allocazione della memoria per i byte di padding per il blocco contenente l'inode del file.\n");
        close(fd);
        return -1;
    }

	ret = write(fd, block_padding, nbytes);

	if (ret != nbytes)
    {
		printf("Errore nella scrittura dei byte di padding nel blocco dell'inode del file.\n");
		close(fd);
		return -1;
	}


	printf("Il blocco contenente l'inode del file è stato scritto con successo.\n");

    fflush(stdout);

	/* Popolo i blocchi di stato del device */
    for(i=0; i<nblocks_state; i++)
    {
        block_state = (int64_t *)malloc(sizeof(int64_t) * 512);

        if(block_state==NULL)
        {
            printf("Errore malloc() iterazione %d\n.", i);
            return 1;
        }

        for(j=0;j<512;j++)
        {
            /* Inizialmente sono tutti liberi */
            block_state[j] = 0x0000000000000000;
        }

	    ret = write(fd, (void *)block_state, SOAFS_BLOCK_SIZE);

	    if (ret != SOAFS_BLOCK_SIZE)
        {
		    printf("Errore nella scrittura dei dati per il blocco %d.\n", i);
		    close(fd);
		    return -1;
	    }
    }

	printf("I blocchi di stato sono stati scritti con successo.\n");

    for(i=0; i<nblocks_data; i++)
    {
        block = (struct soafs_block *)malloc(sizeof(struct soafs_block));

        if(block == NULL)
        {
            printf("Errore malloc() iterazione %d\n.", i);
            return 1;
        }

        memset(block, 0, sizeof(struct soafs_block));

        /* Inizialmente non contengono alcun byte valido */
        block->dim = 0;

	    ret = write(fd, block, sizeof(struct soafs_block));

	    if (ret != SOAFS_BLOCK_SIZE)
        {
		    printf("Errore nella scrittura dei dati per il blocco %d.\n", i);
		    close(fd);
		    return -1;
	    }
    }

    printf("I blocchi di dati sono stati scritti con successo.\n");

	close(fd);

	return 0;
}
