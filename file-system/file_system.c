#include <linux/fs.h> /* sb_set_blocksize()-iget_locked()-inode_init_owner()-unlock_new_inode()-kill_block_super()-mount_bdev() */
#include <linux/timekeeping.h>
#include <linux/time.h>/* ktime_get_real_ts64() */
#include <linux/buffer_head.h>/* sb_bread()-brelse() */
#include <linux/dcache.h>/* d_make_root() */
#include <linux/string.h> /* strlen() */
#include "../headers/main_header.h"



static struct super_operations soafs_super_ops = {

};



static struct dentry_operations soafs_dentry_ops = {

};


/* Inizialmente non ho alcun montaggio */
int is_mounted = 0;
char *mount_path = NULL;


static int soafs_fill_super(struct super_block *sb, void *data, int silent) {   

    struct inode *root_inode;
    struct dentry *root_dentry;
    struct soafs_super_block *sb_disk;
    struct timespec64 curr_time;
    struct buffer_head *bh;

    
    /* Imposto la dimensione del superblocco pari a 4KB. */
    if(sb_set_blocksize(sb, SOAFS_BLOCK_SIZE) == 0)
    {
        printk("%s: Errore nel setup della dimensione del superblocco.\n", MOD_NAME);
        return -EIO;
    }
   
    /*
     * Il Superblocco è il primo blocco memorizzato sul device.
     * Di conseguenza, l'indice che deve essere utilizzato con la
     * sb_bread ha valore pari a zero.
     */
    bh = sb_bread(sb, SOAFS_SB_BLOCK_NUMBER);

    if(bh == NULL){
        printk("%s: Errore nella lettura del superblocco.\n", MOD_NAME);
	    return -EIO;
    }

    /* Recupero il superblocco memorizzato sul device. */
    sb_disk = (struct soafs_super_block *)bh->b_data;   

    /* Verifico il valore del magic number presente nel superblocco su device. */
    if(sb_disk->magic != SOAFS_MAGIC_NUMBER){
        printk("%s: Mancata corrispondenza tra i due magic number.\n", MOD_NAME);
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
        printk("%s: Errore nel recupero del root inode.\n", MOD_NAME);
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
        printk("%s: Errore nella creazione della root directory.\n", MOD_NAME);
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

    printk("%s: Il File System 'soafs' è stato smontato con successo.\n", MOD_NAME);

    /* Registro il fatto che il file system è stato smontato. */
    is_mounted = 0;

}



static struct dentry *soafs_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data)
{
    struct dentry *ret;

    if(mount_path != NULL)
    {
        printk("%s: Esiste già un altro montaggio del file system di tipo %s\n", MOD_NAME, fs_type->name);
        return ERR_PTR(-EINVAL);
    }
    
    /* Monta un file system memorizzato su un block device */
    ret = mount_bdev(fs_type, flags, dev_name, data, soafs_fill_super);

    if (unlikely(IS_ERR(ret)))
    {
        printk("%s: Errore durante il montaggio del File System 'soafs'.\n",MOD_NAME);
        is_mounted = 0;
    }
    else
    {
        printk("%s: Montaggio del File System 'soafs' sul device %s avvenuto con successo.\n",MOD_NAME,dev_name);
        /* Registro il fatto che il file system è stato montato. */
        is_mounted = 1;
    }

    printk("%s: provo con il valore data %s\n", MOD_NAME, (char *)data);

    mount_path = (char *)data;

    printk("%s: Lunghezza stringa data %ld\n", MOD_NAME, strlen(mount_path));

    return ret;
}



/*
 * Descrizione della tipologia di File System
 * memorizzato all'interno del dispositivo a blocchi.
 */
struct file_system_type soafs_fs_type = {
	.owner          = THIS_MODULE,
    .name           = "soafs",
    .mount          = soafs_mount,
    .kill_sb        = soafs_kill_sb,
};


