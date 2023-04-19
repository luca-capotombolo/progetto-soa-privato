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
    sbi->update_list_size = sb_disk->update_list_size;

    sb->s_fs_info = sbi;

    printk("%s: Il numero di blocchi del dispositivo è pari a %lld.\n", MOD_NAME, sb_disk->num_block);
    printk("%s: Il numero di blocchi liberi del dispositivo è pari a %lld.\n", MOD_NAME, sb_disk->num_block_free);
    printk("%s: Il numero di blocchi di stato del dispositivo è pari a %lld.\n", MOD_NAME, sb_disk->num_block_state);
    printk("%s: Il numero di blocchi massimo da caricare all'aggiornamento è pari a %lld.\n", MOD_NAME, sb_disk->update_list_size);
    
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



static int set_free_block(void)
{
    struct soafs_sb_info *sbi;
    struct buffer_head *bh;
    struct soafs_super_block * b;
    uint64_t *free_blocks;
    uint64_t index;
    uint64_t num_block_data;
    uint64_t counter;
    uint64_t update_list_size;
    int ret;
    
    sbi = (struct soafs_sb_info *)sb_global->s_fs_info;

    update_list_size = sbi->update_list_size;
    
    free_blocks = (uint64_t *)kzalloc(sizeof(uint64_t) * update_list_size, GFP_KERNEL);

    if(free_blocks == NULL)
    {
        printk("%s: Errore esecuzione della kzalloc() nella ricerca dei blocchi liberi\n", MOD_NAME);
        return 1;
    }

    printk("%s: Inizio ricerca blocchi liberi (al massimo %lld)...\n", MOD_NAME, update_list_size);

    bh = sb_bread(sb_global, SOAFS_SB_BLOCK_NUMBER);    

    if(bh == NULL)
    {
        printk("%s: [ERRORE] La lettura del superblocco è terminata senza successo\n", MOD_NAME);
        return 1;
    }

    b = (struct soafs_super_block *)bh->b_data;

    b->num_block_free = b->num_block_free - num_block_free_used;

    if((num_block_free_used == sbi->num_block_free) && (head_free_block_list == NULL))
    {
        printk("%s: I blocchi liberi a disposizione sono terminati\n", MOD_NAME);
        b->actual_size = 0;
        return 0;
    }

    num_block_data = sbi->num_block - 2 - sbi->num_block_state;

    counter = 0;

    for(index = 0; index < num_block_data; index++)
    {
        if(counter == update_list_size)
        {
            break;
        }

        ret = check_bit(index);

        if(!ret)
        {
            printk("%s: [FREE BLOCK] Blocco libero #%lld\n", MOD_NAME, index);
            
            free_blocks[counter] = index;

            counter++;
        }

    }

    printk("%s: Numero di blocchi liberi trovati %lld\n", MOD_NAME, counter);

    b->actual_size = counter;
    
    for(index = 0; index < counter; index++)
    {
        b-> index_free[index] = free_blocks[index];
    }

    mark_buffer_dirty(bh);

    ret = sync_dirty_buffer(bh);
    
    brelse(bh);

    kfree(free_blocks);

    return 0;   
    
}




int flush_bitmask(void)
{
    struct soafs_sb_info *sbi;
    struct buffer_head *bh;
    uint64_t counter;
    uint64_t num_block_state;
    uint64_t *b;
    int i;

    sbi = (struct soafs_sb_info *)sb_global->s_fs_info;

    num_block_state = sbi->num_block_state;

    counter = 0;

    while(counter < num_block_state)
    {
        bh = sb_bread(sb_global, 2 + counter);

        if(bh == NULL)
        {
            printk("%s: Errore nella lettura del blocco di stato #%lld\n", MOD_NAME, counter);
            return 1;
        }

        b = (uint64_t *)bh->b_data;

        for(i = 0; i < 512; i++)
        {
            b[i] = bitmask[counter][i];
        }

        mark_buffer_dirty(bh);

        sync_dirty_buffer(bh);

        printk("%s: [FLUSH BITMASK] Flush dei dati per il blocco di stato #%lld avvenuto con successo\n", MOD_NAME, counter);        
        
        counter++;

        brelse(bh);
    }

    return 0;
}



int flush_valid_block(void)
{
    struct buffer_head *bh;
    struct block *curr;
    struct soafs_sb_info *sbi;
    struct soafs_block *b;
    uint64_t index;
    uint64_t num_block_state;
    uint64_t pos;

    curr = head_sorted_list;

    if(curr == NULL)
    {
        printk("%s: Non ci sono messaggi validi da riportare\n", MOD_NAME);
        return 0;
    }

    sbi = (struct soafs_sb_info *)sb_global->s_fs_info;

    num_block_state = sbi->num_block_state;
    
    pos = 0;
    
    while(curr != NULL)
    {
        index = curr->block_index;
    
        printk("%s: [VALID BLOCK] Il blocco con indice %lld deve essere riportato su device\n", MOD_NAME, index);

        bh = sb_bread(sb_global, 2 + num_block_state + index);

        b = (struct soafs_block *)bh->b_data;

        b->pos = pos;

        strncpy(b->msg, curr->msg, strlen(curr->msg) + 1);

        mark_buffer_dirty(bh);

        sync_dirty_buffer(bh);        

        curr = curr->sorted_list_next;

        pos++;

        brelse(bh);
    }

    printk("%s: [VALID BLOCK] I blocchi sono stati riportati correttamente su device\n", MOD_NAME);
    
    return 0;
}



void free_all_memory(void)
{
    struct block_free *curr_bf;
    struct block_free *next_bf;
    struct block *curr_b;
    struct block *next_b;
    int index;

    curr_bf = head_free_block_list;

    if(curr_bf!=NULL)
    {
        while(curr_bf!=NULL)
        {
            next_bf = curr_bf->next;
        
            kfree(curr_bf);
    
            curr_bf = next_bf;
        }
    }

    for(index=0;index<x;index++)
    {
        (&hash_table_valid[index])->head_list = NULL;
    }

    curr_b = head_sorted_list;

    if(curr_b!=NULL)
    {
        while(curr_b!=NULL)
        {
            next_b = curr_b->sorted_list_next;
        
            kfree(curr_b);
    
            curr_b = next_b;
        }
    }    
}

static void soafs_kill_sb(struct super_block *sb)
{

    int ret;
    int n;

    is_mounted = 0;
    n = 0;

retry_flush_bitmask:

    ret = flush_bitmask();

    if(ret)
    {
        n++;
        printk("%s: Tentativo numero %d fallito per il flush della bitmask\n", MOD_NAME, n);
        goto retry_flush_bitmask;
    }

    n = 0;

retry_set_free_block:

    ret = set_free_block();

    if(ret)
    {
        n++;
        printk("%s: Tentativo numero %d fallito per il settaggio dei blocchi liberi\n", MOD_NAME, n);
        goto retry_set_free_block;
    }

    n = 0;

retry_umount:

    ret = flush_valid_block();

    if(ret)
    {
        n++;
        printk("%s: Tentativo numero %d fallito per il salvataggio dei blocchi validi\n", MOD_NAME, n);
        goto retry_umount;
    }

    free_all_memory();

    kill_block_super(sb);

    printk("%s: Il File System 'soafs' è stato smontato con successo.\n", MOD_NAME);
    

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


