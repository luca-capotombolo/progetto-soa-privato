//file_system.h

#ifndef _FILE_SYSTEM_H
#define _FILE_SYSTEM_H



#include <linux/types.h>        /* uint64_t */



/* Magic Number */
#define SOAFS_MAGIC_NUMBER 0xabababab
/* Taglia del blocco */
#define SOAFS_BLOCK_SIZE 4096
/* Numero del blocco contenente il super blocco */
#define SOAFS_SB_BLOCK_NUMBER 0
/* Numero del blocco contenente l'inode del file */
#define DEFAULT_FILE_INODE_BLOCK 1
/* Lunghezza massima del nome */
#define SOAFS_MAX_NAME_LEN		16
/* Numero del root inode */
#define SOAFS_ROOT_INODE_NUMBER 10
/* Numero del file inode */
#define SOAFS_FILE_INODE_NUMBER 1
/* Numero del blocco contenente l'inode */
#define SOAFS_INODE_BLOCK_NUMBER 1
/* Nome dell'unico file */
#define SOAFS_UNIQUE_FILE_NAME "the-file"
/* Numero massimo di blocchi supportato dal device */
#define NBLOCKS 20
/* Versione del File System */
#define VERSION 1
/* Dimensione in bytes dei metadati da utilizzare */
#define METADATA 8
/* Maschera di bit per recuperare la validità del blocco */
#define MASK_VALID 0x8000000000000000
/* Maschera di bit per recuperare la validità del blocco */
#define MASK_POS 0x0fffffffffffffff
/* Numero di blocchi che non contengono dati */
#define NUM_NODATA_BLOCK 2
/* Numero di Epoche */
#define EPOCHS 2



/*
 * Rappresenta il superblocco mantenuto sul device.
 * Il superblocco è memorizzato all'interno del primo
 * blocco del device.
 */
struct soafs_super_block {
	uint64_t version;
	uint64_t magic;
	uint64_t block_size;
    uint64_t num_block;         //Mi consente di fare il checks rispetto a NBLOCKS
	char padding[SOAFS_BLOCK_SIZE - (4 * sizeof(uint64_t))];  
};



/* Directory Entry 
struct soafs_dir_entry {
	char filename[SOAFS_MAX_NAME_LEN];
	uint64_t inode_no;
};*/



/* inode */
struct soafs_inode {
	mode_t mode;
    uid_t uid;                                                  //Non ancora utilizzato nella formattazione.
    gid_t gid;                                                  //Non ancora utilizzato nella formattazione.
	uint64_t inode_no;
	uint64_t data_block_number;                                 //not exploited
	union {
		uint64_t file_size;
		uint64_t dir_children_count;
	};
};



struct soafs_block {
    /*
     * L'ultimo bit viene utilizzato per stabilire se il blocco
     * contiene o meno un contenuto valido. I restanti bit vengono
     * utilizzati per stabilire la posizione nell'ordine di consegna
     * dei blocchi.
     */
    uint64_t metadata;
    char msg[SOAFS_BLOCK_SIZE - (sizeof(uint64_t))];
};



/*
 * La struttura dati 'block' rappresenta un singolo
 * elemento che viene inserito all'interno di una delle
 * liste nella hash_table_valid. Rappresenta un blocco
 * del device che mantiene un messaggio valido.
 */
struct block {
    uint64_t block_index;
    uint64_t pos;
    char *msg;
    struct block* hash_table_next;
    struct block* sorted_list_next;
};



/*
 * La struttura dati 'block_free' rappresenta un singolo
 * elemento che viene inserito all'interno della lista
 * free_block_list che mantiene le informazioni sui
 * blocchi liberi.
 */
struct block_free {
    uint64_t block_index;
    struct block_free *next;
};



/*
 * La struttura dati 'ht_valid_entry' rappresenta una
 * entry della hash table hash_table_valid.
 */
struct ht_valid_entry {
    unsigned long standing[EPOCHS];
    unsigned long epoch;
//    struct mutex entry_mutex;
    struct block *head_list;
    int next_epoch_index;
};



extern struct block *head_sorted_list;                          //file_system.c
extern struct block_free *head_free_block_list;                      //file_system.c
extern struct ht_valid_entry *hash_table_valid;                //file_system.c

extern int check_is_mounted(void);                              //file_system.c
extern struct file_system_type soafs_fs_type;                   //file_system.c
extern int is_mounted;                                          //file_system.c
extern char *mount_path;                                        //file_system.c
extern struct super_block * sb_global;                          //file_system.c

extern const struct inode_operations soafs_inode_ops;           // file.c
extern const struct file_operations soafs_file_operations;      // file.c

extern const struct file_operations soafs_dir_operations;       // dir.c



#endif /* _FILE_SYSTEM_H */
