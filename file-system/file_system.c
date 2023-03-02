#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/timekeeping.h>
#include <linux/time.h>
#include "file_system.h"



MODULE_LICENSE("GPL");
MODULE_AUTHOR("Luca Capotombolo <capoluca99@gmail.com>");
MODULE_DESCRIPTION("FILE-SYSTEM-SOA");

/*
 * Funzione: soafs_iget
 * -----------------------------
 *      Alloca il VFS inode (i.e., struct inode) e popola la struttura dati
 *      con le informazioni specifiche dell'inode (i.e., struct soafs_inode)
 *      memorizzate sul device.
 *
 *      Return: Il puntatore al VFS inode allocato e popolato.
 */
struct inode * soafs_iget(struct super_block *sb, unsigned long ino)
{
    
}

static int soafs_fill_super(struct super_block *sb, void *data, int silent) {   

    struct inode *root_inode;
    struct dentry *root_dentry;

    struct soafs_super_block *sb_disk;
    uint64_t magic;

    struct buffer_head *bh;
    struct timespec64 curr_time;
    

    sb->s_magic = SOAFS_MAGIC;
   
    /*
     * Il Super Blocco è il primo blocco sul device.
     * Di conseguenza, l'indice che deve essere utilizzato
     * ha valore pari a zero.
     */
    bh = sb_bread(sb, SB_BLOCK_NUMBER);

    if(!sb){

	    return -EIO;

    }
    sb_disk = (struct onefilefs_sb_info *)bh->b_data;
    magic = sb_disk->magic;
    brelse(bh);

    //check on the expected magic number
    if(magic != sb->s_magic){
	return -EBADF;
    }

    sb->s_fs_info = NULL; //FS specific data (the magic number) already reported into the generic superblock
    sb->s_op = &singlefilefs_super_ops;//set our own operations


    root_inode = iget_locked(sb, 0);//get a root inode indexed with 0 from cache
    if (!root_inode){
        return -ENOMEM;
    }

    root_inode->i_ino = SINGLEFILEFS_ROOT_INODE_NUMBER;//this is actually 10 oppure 0????
    inode_init_owner(&init_user_ns, root_inode, NULL, S_IFDIR);//set the root user as owned of the FS root
    root_inode->i_sb = sb;
    root_inode->i_op = &onefilefs_inode_ops;//set our inode operations
    root_inode->i_fop = &onefilefs_dir_operations;//set our file operations
    //update access permission
    root_inode->i_mode = S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IXUSR | S_IXGRP | S_IXOTH;

    //baseline alignment of the FS timestamp to the current time
    ktime_get_real_ts64(&curr_time);
    root_inode->i_atime = root_inode->i_mtime = root_inode->i_ctime = curr_time;

    // no inode from device is needed - the root of our file system is an in memory object
    root_inode->i_private = NULL;

    sb->s_root = d_make_root(root_inode);
    if (!sb->s_root)
        return -ENOMEM;

    sb->s_root->d_op = &singlefilefs_dentry_ops;//set our dentry operations

    //unlock the inode to make it usable
    unlock_new_inode(root_inode);

    return 0;
}

static void soafs_kill_superblock(struct super_block *sb)
{
    kill_block_super(sb);

    printk(LOG_LEVEL "Il File System 'soafs' è stato smontato con successo.", MOD_NAME);

}

static struct dentry *soafs_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data)
{
    struct dentry *ret;

    ret = mount_nodev(fs_type, flags, data, soafs_fill_super);

    if (unlikely(IS_ERR(ret)))

        printk(LOG_LEVEL "%s: Errore durante il montaggio del File System 'soafs'.",MOD_NAME);

    else

        printk(LOG_LEVEL "%s: Montaggio del File System 'soafs' sul device %s avvenuto con successo. %s\n",MOD_NAME,dev_name);

    return ret;
}

static int soafs_init(void) {

    int ret;

    ret = register_filesystem(&soafs_fs_type);

    if (likely(ret == 0))

        printk(LOG_LEVEL "%s: File System 'soafs' registrato correttamente.",MOD_NAME);

    else

        printk(LOG_LEVEL "%s: Errore nella registrazione del File System 'soafs'. - Errore: %d", MOD_NAME,ret);

    return ret;
}

static void soafs_exit(void) {

    int ret;

    ret = unregister_filesystem(&soafs_fs_type);

    if (likely(ret == 0))

        printk(LOG_LEVEL "%s: Rimozione della tipologia di File System 'soafs' avvenuta con successo.",MOD_NAME);

    else

        printk(LOG_LEVEL "%s: Errore nella rimozione della tipologia di File System 'soafs' - Errore: %d", MOD_NAME, ret);

}

module_init(soafs_init);
module_exit(soafs_exit);
