#include <linux/fs.h>           /* sb_set_blocksize()-iget_locked()-inode_init_owner()-unlock_new_inode()-kill_block_super()-mount_bdev() */
#include <linux/timekeeping.h>
#include <linux/time.h>         /* ktime_get_real_ts64() */
#include <linux/buffer_head.h>  /* sb_bread()-brelse() */
#include <linux/dcache.h>       /* d_make_root() */
#include <linux/string.h>       /* strlen() */
#include <linux/log2.h>         /* ilog2()  */
#include <linux/slab.h>         /* kmalloc() */

#include "../headers/main_header.h"



int is_mounted = 0;                                             /* Inizialmente non ho alcun montaggio. */
struct super_block *sb_global = NULL;                           /* Riferimento al superblocco. */


static struct super_operations soafs_super_ops = {

};



static struct dentry_operations soafs_dentry_ops = {

};



/*
 * Poiché per semplicità si è assunto di lavorare
 * con una singola istanza di file system 'soafs',
 * questa funzione verifica se è stata montata tale
 * istanza.
 */
int check_is_mounted(void)
{
    if(!is_mounted)
    {
        return 0;
    }

    return 1;
}


static int soafs_fill_super(struct super_block *sb, void *data, int silent) {   

    struct inode *root_inode;
    struct dentry *root_dentry;
    struct soafs_super_block *sb_disk;
    struct timespec64 curr_time;
    struct buffer_head *bh;
    struct soafs_sb_info *sbi; 
    int ret;

    
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

    /* Faccio il check per verificare se il numero di blocchi nel dispositivo è valido. */
    if( (sb_disk->num_block > NBLOCKS) || ((sb_disk->num_block - sb_disk->num_block_state - 2) <= 0) )
    {
        printk("%s: Il numero di blocchi del dispositivo non è valido.\n", MOD_NAME);
        brelse(bh);
        return -EINVAL;
    }

    /* Verifico il valore del magic number presente nel superblocco sul device. */
    if(sb_disk->magic != SOAFS_MAGIC_NUMBER){
        printk("%s: Mancata corrispondenza tra i due magic number.\n", MOD_NAME);
        brelse(bh);
	    return -EBADF;
    }

    /* Popolo la struttura dati del superblocco generico. */
    sb->s_magic = SOAFS_MAGIC_NUMBER;
    sb->s_op = &soafs_super_ops;

    /* Recupero informazioni FS specific. */
    sbi = (struct soafs_sb_info *)kzalloc(sizeof(struct soafs_sb_info), GFP_KERNEL);

    if(sbi == NULL)
    {
        printk("%s: Errore kzalloc() nell'allocazione della struttura dati soafs_sb_info.\n", MOD_NAME);
        brelse(bh);
        return -ENOMEM;
    }

    sbi->num_block = sb_disk->num_block;
    sbi->num_block_free = sb_disk->num_block_free;
    sbi->num_block_state = sb_disk->num_block_state;

    sb->s_fs_info = sbi;

    printk("%s: Il numero di blocchi del dispositivo è pari a %lld.\n", MOD_NAME, sb_disk->num_block);
    printk("%s: Il numero di blocchi liberi del dispositivo è pari a %lld.\n", MOD_NAME, sb_disk->num_block_free);
    printk("%s: Il numero di blocchi di stato del dispositivo è pari a %lld.\n", MOD_NAME, sb_disk->num_block_state);
    
    /* Recupero il root inode. */
    root_inode = iget_locked(sb, 0);

    if(!root_inode)
    {
        printk("%s: Errore nel recupero del root inode.\n", MOD_NAME);
        brelse(bh);
        kfree(sbi);
        return -ENOMEM;
    }

    root_inode->i_ino = SOAFS_ROOT_INODE_NUMBER;
    inode_init_owner(&init_user_ns, root_inode, NULL, S_IFDIR);
    root_inode->i_sb = sb;
    root_inode->i_mode = S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IXUSR | S_IXGRP | S_IXOTH;
    ktime_get_real_ts64(&curr_time);
    root_inode->i_atime = root_inode->i_mtime = root_inode->i_ctime = curr_time;
    root_inode->i_private = NULL;
    root_inode->i_op = &soafs_inode_ops;
    root_inode->i_fop = &soafs_dir_operations;

    /* Recupero la root dentry. */
    root_dentry = d_make_root(root_inode);

    if (!root_dentry)
    {
        printk("%s: Errore nella creazione della root directory.\n", MOD_NAME);
        brelse(bh);
        kfree(sbi);
        return -ENOMEM;
    }

    sb->s_root = root_dentry;
    sb->s_root->d_op = &soafs_dentry_ops;

    unlock_new_inode(root_inode);

    /* Prendo il riferimento al superblocco */
    sb_global = sb;

    ret = init_data_structure_core(sb_disk->num_block - sb_disk->num_block_state - 2, sb_disk->index_free, sb_disk->actual_size);

    if(ret)
    {
        printk("%s: Errore nella inizializzazione delle strutture dati core del modulo.\n", MOD_NAME);
        brelse(bh);
        kfree(sbi);
        return -EIO;
    }

    /* Rilascio del superblocco */
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

    if(is_mounted)
    {
        printk("%s: Esiste già un altro montaggio del file system di tipo %s\n", MOD_NAME, fs_type->name);
        return ERR_PTR(-EINVAL);
    }
    
    ret = mount_bdev(fs_type, flags, dev_name, data, soafs_fill_super);         /* Monta un file system memorizzato su un block device */

    if (unlikely(IS_ERR(ret)))
    {
        printk("%s: Errore durante il montaggio del File System 'soafs'.\n",MOD_NAME);
        is_mounted = 0;
    }
    else
    {
        printk("%s: Montaggio del File System 'soafs' sul device %s avvenuto con successo.\n",MOD_NAME,dev_name);
        is_mounted = 1;     /* Registro il fatto che il file system è stato montato. */
    }

    return ret;
}



struct file_system_type soafs_fs_type = {
	.owner          = THIS_MODULE,
    .name           = "soafs",
    .mount          = soafs_mount,
    .kill_sb        = soafs_kill_sb,
};


