#ifndef _FILE_SYSTEM_H
#define _FILE_SYSTEM_H



#include <linux/types.h>/* uint64_t */



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



/*
 * Rappresenta il superblocco mantenuto sul device.
 * Il superblocco è memorizzato all'interno del primo
 * blocco del device.
 */
struct soafs_super_block {
	uint64_t version;
	uint64_t magic;
	uint64_t block_size;
    uint64_t num_block;
	char padding[SOAFS_BLOCK_SIZE - (4 * sizeof(uint64_t))];  
};



/* Directory Entry */
struct soafs_dir_entry {
	char filename[SOAFS_MAX_NAME_LEN];
	uint64_t inode_no;
};



/* inode */
struct soafs_inode {
	mode_t mode;
    uid_t uid;
    gid_t gid;
	uint64_t inode_no;
	uint64_t data_block_number;//not exploited
	union {
		uint64_t file_size;
		uint64_t dir_children_count;
	};
};



// file.c
extern const struct inode_operations soafs_inode_ops;
extern const struct file_operations soafs_file_operations; 
// dir.c
extern const struct file_operations soafs_dir_operations;
//file_system.c
extern struct file_system_type soafs_fs_type;

#endif /* _FILE_SYSTEM_H */
