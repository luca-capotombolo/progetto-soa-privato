#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/timekeeping.h>
#include <linux/time.h>
#include <linux/buffer_head.h>

#include "./header/file_system.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Luca Capotombolo <capoluca99@gmail.com>");
MODULE_DESCRIPTION("FILE-SYSTEM-SOA");

static struct super_operations soafs_super_ops = {

};

static struct dentry_operations soafs_dentry_ops = {

};

static int soafs_fill_super(struct super_block *sb, void *data, int silent) {   

    struct inode *root_inode;
    struct dentry *root_dentry;
    struct soafs_super_block *sb_disk;
    struct timespec64 curr_time;
    struct buffer_head *bh;
    
    /* Imposto la dimensione del superblocco pari a 4KB. */
    if(sb_set_blocksize(sb, SOAFS_BLOCK_SIZE) == 0)
    {
        printk("%s: Errore nel setup della dimensione del superblocco.", MOD_NAME);
        return -EIO;
    }
   
    /*
     * Il Superblocco è il primo blocco memorizzato sul device.
     * Di conseguenza, l'indice che deve essere utilizzato con la
     * sb_bread ha valore pari a zero.
     */
    bh = sb_bread(sb, SOAFS_SB_BLOCK_NUMBER);

    if(bh == NULL){
        printk("%s: Errore nella lettura del superblocco.", MOD_NAME);
	    return -EIO;
    }

    /* Recupero il superblocco memorizzato sul device. */
    sb_disk = (struct soafs_super_block *)bh->b_data;   

    /* Verifico il valore del magic number presente nel superblocco su device. */
    if(sb_disk->magic != SOAFS_MAGIC_NUMBER){
        printk("%s: Mancata corrispondenza tra i due magic number.", MOD_NAME);
        /* Rilascio il buffer cache che mantiene il superblocco. */
        brelse(bh);
	    return -EBADF;
    }

    /* Popolo la struttura dati del superblocco generico. */
    sb->s_magic = SOAFS_MAGIC_NUMBER;
    sb->s_op = &soafs_super_ops;

    /* I dati specifici del File System sono stati già riportati nel superblocco generico. */
    sb->s_fs_info = NULL;

    //TODO: Decommenta il codice se si necessita di dati specifici del superblocco del FS.

    /* sbi = kzalloc(sizeof(struct minfs_sb_info), GFP_KERNEL);
	if (!sbi)
    {
		return -ENOMEM;
    }
	s->s_fs_info = sbi */

    /* Recupero il root inode. */
    root_inode = iget_locked(sb, 0);

    if(!root_inode)
    {
        printk("%s: Errore nel recupero del root inode.", MOD_NAME);
        brelse(bh);
        return -ENOMEM;
    }

    root_inode->i_ino = SOAFS_ROOT_INODE_NUMBER;
    inode_init_owner(&init_user_ns, root_inode, NULL, S_IFDIR);
    root_inode->i_sb = sb;

    root_inode->i_mode = S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IXUSR | S_IXGRP | S_IXOTH;

    ktime_get_real_ts64(&curr_time);
    root_inode->i_atime = root_inode->i_mtime = root_inode->i_ctime = curr_time;

    //TODO: Vedi se necessita delle informazioni, non è necessariamente l'inode della root.
    root_inode->i_private = NULL;

    //TODO: Vedi se bisogna implementare specifiche operazioni.
    root_inode->i_op = &soafs_inode_ops;
    root_inode->i_fop = &soafs_dir_operations;

    /* Recupero la root dentry. */
    root_dentry = d_make_root(root_inode);

    if (!root_dentry)
    {
        printk("%s: Errore nella creazione della root directory.", MOD_NAME);
        brelse(bh);
        return -ENOMEM;
    }

    sb->s_root = root_dentry;

    sb->s_root->d_op = &soafs_dentry_ops;

    unlock_new_inode(root_inode);
    
    brelse(bh);

    return 0;
}

static void soafs_kill_sb(struct super_block *sb)
{
    kill_block_super(sb);

    printk("%s: Il File System 'soafs' è stato smontato con successo.", MOD_NAME);

}

static struct dentry *soafs_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data)
{
    struct dentry *ret;

    /* Monta un file system memorizzato su un block device */
    ret = mount_bdev(fs_type, flags, dev_name, data, soafs_fill_super);

    if (unlikely(IS_ERR(ret)))
    {
        printk("%s: Errore durante il montaggio del File System 'soafs'.",MOD_NAME);
    }
    else
    {
        printk("%s: Montaggio del File System 'soafs' sul device %s avvenuto con successo.",MOD_NAME,dev_name);
    }

    return ret;
}

static struct file_system_type soafs_fs_type = {
	.owner          = THIS_MODULE,
    .name           = "soafs",
    .mount          = soafs_mount,
    .kill_sb        = soafs_kill_sb,
};

static int soafs_init(void)
{

    int ret;

    ret = register_filesystem(&soafs_fs_type);

    if (likely(ret == 0))
    {
        printk("%s: File System 'soafs' registrato correttamente.\n",MOD_NAME);
    }    
    else
    {
        printk("%s: Errore nella registrazione del File System 'soafs'. - Errore: %d", MOD_NAME,ret);
    }

    return ret;
}

static void soafs_exit(void)
{

    int ret;

    ret = unregister_filesystem(&soafs_fs_type);

    if (likely(ret == 0))
    {
        printk("%s: Rimozione della tipologia di File System 'soafs' avvenuta con successo.\n",MOD_NAME);
    }
    else
    {
        printk("%s: Errore nella rimozione della tipologia di File System 'soafs' - Errore: %d", MOD_NAME, ret);
    }

}

module_init(soafs_init);
module_exit(soafs_exit);
