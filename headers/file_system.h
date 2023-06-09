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
#define NBLOCKS 10000000

/* Numero di Epoche */
#define EPOCHS 2

/* Numero massimo di blocchi liberi con cui inizializzo la free_block_list */
#define SIZE_INIT 10



/**
 * @magic: Magic Number
 * @num_block: Numero totale di blocchi del device
 * @num_block_free: Numero totale dei blocchi liberi al montaggio
 * @num_block_state: Numero totale dei blocchi di stato
 * @update_list_size: Numero massimo di nuovi blocchi da caricare
 * @actual_size: Dimensione effettiva dell'array di indici liberi 'index_free'
 * @index_free: Sottoinsieme dei blocchi liberi al montaggio
 * @padding: Bit di Padding
 *
 * Rappresenta il superblocco mantenuto sul device. Il superblocco è memorizzato
 * all'interno del primo blocco del device.
 */
struct soafs_super_block {
	uint64_t magic;
    uint64_t num_block;
    uint64_t num_block_free;
    uint64_t num_block_state;
    uint64_t update_list_size;
    uint64_t actual_size;
    uint64_t index_free[SIZE_INIT];
	char padding[SOAFS_BLOCK_SIZE - ((6 + SIZE_INIT) * sizeof(uint64_t))];
};



/**
 * @pos: Posizione del blocco all'interno della lista ordinata
 * @dim: Numero di byte validi mantenuti all'interno del blocco
 * @msg: Messaggio utente contenuto all'interno del blocco
 *
 * Rappresenta il contenuto di un singolo blocco del dispositivo.
 */
struct soafs_block {
    uint64_t pos;
    unsigned short dim;
    char msg[SOAFS_BLOCK_SIZE - (sizeof(uint64_t) + sizeof(unsigned short))];
};



/**
 * @num_block: Numero dei blocchi del dispositivo
 * @num_block_free: Numero dei blocchi liberi nel dispositivo
 * @num_block_state: Numero dei blocchi di stato nel dispositivo
 * @update_list_size: Numero massimo di nuovi blocchi da caricare
 *
 * Mantiene le informazioni specifiche del SB del FS.
 */
struct soafs_sb_info {
    uint64_t num_block;
    uint64_t num_block_free;
    uint64_t num_block_state;
    uint64_t update_list_size;
};



/* inode */
struct soafs_inode {
	mode_t mode;
	uint64_t inode_no;
	uint64_t data_block_number;
	union {
		uint64_t file_size;
		uint64_t dir_children_count;
	};
};



extern void free_all_memory(void);                              //file_system.c
extern void wake_up_umount(void);                               //file_system.c

extern int is_mounted;                                          //file_system.c
extern struct file_system_type soafs_fs_type;                   //file_system.c
extern struct super_block * sb_global;                          //file_system.c


extern const struct inode_operations soafs_inode_ops;           // file.c
extern const struct file_operations soafs_file_operations;      // file.c

extern const struct file_operations soafs_dir_operations;       // dir.c



#endif /* _FILE_SYSTEM_H */
