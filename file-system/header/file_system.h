#ifndef _SOAFS_H
#define _SOAFS_H    1

#include <linux/types.h>

#define MOD_NAME "File-System-Soa"

#define SOAFS_MAGIC_NUMBER 0xabababab
#define SOAFS_BLOCK_SIZE 4096
#define SOAFS_SB_BLOCK_NUMBER 0
#define DEFAULT_FILE_INODE_BLOCK 1

#define SOAFS_MAX_NAME_LEN		16

#define SOAFS_ROOT_INODE_NUMBER 10
#define SOAFS_FILE_INODE_NUMBER 1
#define SOAFS_INODE_BLOCK_NUMBER 1

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

struct soafs_dir_entry {
	char filename[SOAFS_MAX_NAME_LEN];
	uint64_t inode_no;
};

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

#endif /* _SOAFS_H */
