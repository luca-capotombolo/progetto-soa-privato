#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../headers/file_system.h"

int main(int argc, char *argv[])
{
	int fd, nbytes, nblocks, i, n;
	ssize_t ret;
	char *block_padding;
	char *file_body = "Blocco #%d.";
	struct soafs_super_block sb;
	struct soafs_inode file_inode;
	//struct soafs_dir_entry record; 
    struct soafs_block *block = NULL;
    uint64_t metadata = 0x8000000000000000;

	if (argc != 3) {
		printf("Invocazione non corretta.\nSi deve eseguire: ./singlefilemakefs <device> <NBLOCKS>\n");
		return -1;
	}

    /* Recupero il device da aprire */
	fd = open(argv[1], O_RDWR);
	if (fd == -1) {
		perror("Errore nell'apertura del device.\n");
		return -1;
	}

    printf("Il nome del device è: %s\n", argv[1]);

    /* Recupero il numero di blocchi del device */
    nblocks = atoi(argv[2]);
    if(nblocks > NBLOCKS)
    {
        printf("E' stato inserito un numero di blocchi scorretto.\nIl numero di blocchi deve essere non superiore a %d\n", NBLOCKS);
        return -1;
    }

    printf("Numero di blochi del device: %d\n", nblocks);
    fflush(stdout);

	/* Popolo il contenuto del superblocco */

    /* Versione del file system */
	sb.version = VERSION;

    /* Magic Number del File System */
	sb.magic = SOAFS_MAGIC_NUMBER;

    /* Dimensione del blocco */
	sb.block_size = SOAFS_BLOCK_SIZE;

    /* Numero di blocchi */
    sb.num_block = nblocks;

	ret = write(fd, (char *)&sb, sizeof(sb));

	if (ret != SOAFS_BLOCK_SIZE) {
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

    /* Numero di blocchi dati associati al file.
     * Devo togliere il blocco contenente l'inode
     * del file e il blocco contenente il superblocco.
     */
    file_inode.data_block_number = nblocks - NUM_NODATA_BLOCK;

    /*
     * La dimensione del file è pari al contenuto di tutti i
     * blocchi di dati che sono presenti all'interno del block
     * device con contenuto valido. Inizialmente, assumo che
     * tutti i blocchi hanno un contenuto valido.
     */
	file_inode.file_size = (strlen(file_body) + 1) * file_inode.data_block_number;

	printf("La dimensione del file è %ld\n",file_inode.file_size);
	fflush(stdout);

    /* Scrittura dei dati effettivi dell'inode */
	ret = write(fd, (char *)&file_inode, sizeof(file_inode));
	if (ret != sizeof(file_inode)) {
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
	if (ret != nbytes) {
		printf("Errore nella scrittura dei byte di padding nel blocco dell'inode del file.\n");
		close(fd);
		return -1;
	}


	printf("Il blocco contenente l'inode del file è stato scritto con successo.\n");
    fflush(stdout);

	/* Popolo i blocchi del device che contengono i dati del file. */

    /* 
     * All'interno del buffer inserisco il contenuto dei vari blocchi
     * che si differenzia a seconda dell'identificativo del blocco.
     */

    /* Processo di scrittura dei blocchi di dati del file */
    for(i=0; i<(nblocks - NUM_NODATA_BLOCK); i++)
    {
        block = (struct soafs_block *)malloc(sizeof(struct soafs_block));

        if(block==NULL)
        {
            printf("Errore malloc() iterazione %d\n.", i);
            return 1;
        }

        /* Azzero tutti il contenuto della struttura dati */
        memset(block, 0, sizeof(struct soafs_block));

        /* Setto i metadati relativi alla validità del blocco e alla posizione nell'ordinamento */
        block->metadata = metadata;

        metadata += 1;
#ifdef ALTERNATO
        if(i%2==0)
            metadata = metadata & MASK_POS;
        else
            metadata = metadata | MASK_VALID;
#endif
        
        sprintf(block->msg, file_body, i);

        //printf("Contenuto del buffer %d: %s\n", i, block->msg);

        /* Scrittura del contenuto effettivo del blocco */
	    ret = write(fd, block, sizeof(struct soafs_block));

	    if (ret != SOAFS_BLOCK_SIZE) {
		    printf("Errore nella scrittura dei dati per il blocco %d.\n", i);
		    close(fd);
		    return -1;
	    }
    }

	printf("I blocchi di dati del file sono stati scritti con successo.\n");

	close(fd);

	return 0;
}
