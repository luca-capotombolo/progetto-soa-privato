#ifndef _SOAFS_H
#define _SOAFS_H    1

#include <linux/types.h>

#define LOG_LEVEL KERN_ALERT
#define MOD_NAME "File-System-Soa"

#define SOAFS_MAGIC 0xabababab
#define SOAFS_BLOCK_SIZE 4096

#define SB_BLOCK_NUMBER 0

struct soafs_super_block {
	uint64_t version;
	uint64_t magic;
	uint64_t block_size;
	uint64_t inodes_count;//not exploited
	uint64_t free_blocks;//not exploited
	char padding[SOAFS_BLOCK_SIZE - (5 * sizeof(uint64_t))];  
};

struct soafs_dir_entry {
	char filename[FILENAME_MAXLEN];
	uint64_t inode_no;
}

struct soafs_inode {
	mode_t mode;
	uint64_t inode_no;
	uint64_t data_block_number;//not exploited
	union {
		uint64_t file_size;
		uint64_t dir_children_count;
	};
};

struct file_system_type soafs_fs_type = {
	.owner          = THIS_MODULE,
    .name           = "soafs",
    .mount          = soafs_mount,
    .kill_sb        = soafs_kill_superblock,
};

#endif /* _SOAFS_H */
