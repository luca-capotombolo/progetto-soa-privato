#include <linux/fs.h> /* sb_set_blocksize()-iget_locked()-inode_init_owner()-unlock_new_inode()-kill_block_super()-mount_bdev() */
#include <linux/timekeeping.h>
#include <linux/time.h>/* ktime_get_real_ts64() */
#include <linux/buffer_head.h>/* sb_bread()-brelse() */
#include <linux/dcache.h>/* d_make_root() */
#include <linux/string.h> /* strlen() */

#include <linux/log2.h>         /* ilog2()  */
#include <linux/slab.h>         /* kmalloc() */

#include "../headers/main_header.h"



int is_mounted = 0;                                             /* Inizialmente non ho alcun montaggio */
struct super_block *sb_global = NULL;                           /* Riferimento al superblocco */

struct block *head_sorted_list = NULL;                          /* Puntatore alla testa della lista contenente i blocchi nell'ordine di consegna */

struct block_free *head_free_block_list = NULL;                      /* Puntatore alla testa della lista contenente i blocchi liberi */
struct ht_valid_entry *hash_table_valid = NULL;                /* Implementazione della hash table hash_table_valid */



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



/*
 * Computa il numero di righe della tabella hash
 * applicando la formula. Si cerca di avere un numero
 * logaritmico di elementi per ogni lista della
 * hast_table_valid.
 */
int compute_num_rows(uint64_t num_data_block)
{
    int x;
    int list_len;
    
    if(num_data_block == 1)
    {
        return 1;      /* Non devo applicare la formula e ho solamente una lista */
    }
 
    list_len = ilog2(num_data_block) + 1;    /* Computo il numero di elementi massimo che posso avere in una lista */

    if((num_data_block % list_len) == 0)
    {
        x = num_data_block / list_len;
    }
    else
    {
        x = (num_data_block / list_len) + 1;
    }

    printk("%s: La lunghezza massima di una entry della tabella hash è pari a %d.\n", MOD_NAME, list_len);
    
    return x;
}



/*
 * Inserisce un nuovo elemento all'interno della lista
 * dei blocchi ordinata secondo l'ordine di consegna.
 */
void insert_sorted_list(struct block *block)
{
    struct block *prev;
    struct block *curr;

    if(head_sorted_list == NULL)
    {
        /* La lista è vuota */
        head_sorted_list = block;
        block->sorted_list_next = NULL;
    }
    else
    {
        if( (head_sorted_list->pos) > (block->pos) )
        {
            /* Inserimento in testa */
            block->sorted_list_next = head_sorted_list;
            head_sorted_list = block;
        }
        else
        {
            prev = head_sorted_list;
            curr = head_sorted_list->sorted_list_next;

            while(curr!=NULL)
            {
                if((curr->pos) > (block->pos))
                {
                    break;
                }
                prev = curr;
                curr = curr->sorted_list_next;
            }
            
            block->sorted_list_next = curr;
            prev -> sorted_list_next = block;        
        }   
    }

}



/*
 * Inserisce un nuovo elemento all'interno
 * della lista contenente le informazioni
 * relative ai blocchi liberi.
 */
int insert_free_list(uint64_t index)
{
    struct block_free *new_item;
    struct block_free *old_head;

    new_item = (struct block_free *)kmalloc(sizeof(struct block_free), GFP_KERNEL);
    if(new_item==NULL)
    {
        printk("%s: Errore malloc() sorted_list.", MOD_NAME);
        return 1;
    }
    new_item->block_index = index;

    if(head_free_block_list == NULL)
    {
        head_free_block_list = new_item;
        new_item -> next = NULL;
    }
    else
    {
        old_head = head_free_block_list;
        head_free_block_list = new_item;
        new_item -> next = old_head;
    }

    printk("%s: Inserto il blocco %lld nella coda blocchi liberi.\n", MOD_NAME, index);

    asm volatile("mfence");

    return 0;
}



/*
 * Inserisce un nuovo elemento all'interno della lista
 * identificata dal parametro 'x'.
 */
int insert_hash_table_valid(struct soafs_block *data_block, uint64_t pos, uint64_t index, int x)
{
    int num_entry;
    struct block *new_item;
    struct block *old_head;
    struct ht_valid_entry *ht_entry; 

    /* Identifico la lista corretta nella hash table */
    num_entry = index % x;
    ht_entry = &hash_table_valid[num_entry];

    /* Alloco il nuovo elemento da inserire nella lista */
    new_item = (struct block *)kmalloc(sizeof(struct block), GFP_KERNEL);
    
    if(new_item==NULL)
    {
        printk("Errore malloc() inserimento hash table.");
        return 1;
    }

    /* Inizializzo il nuovo elemento */
    new_item->block_index = index;

    new_item->pos = pos;

    new_item->msg = (char *)kmalloc(strlen(data_block->msg) + 1, GFP_KERNEL);

    if(new_item->msg==NULL)
    {
        printk("Errore malloc() stringa inserimento hash table.");
        return 1;
    }

    printk("%s: Stringa da copiare - %s.\n", MOD_NAME, data_block->msg);

    strncpy(new_item->msg, data_block->msg, strlen(data_block->msg) + 1);

    printk("Stringa copiata per il blocco di indice %lld: %s\n", index, new_item->msg);

    /* Inserimento in testa */
    if(ht_entry->head_list == NULL)
    {
        /* La lista è vuota */
        ht_entry->head_list = new_item;
        ht_entry->head_list->hash_table_next = NULL;
    }
    else
    {
        old_head = ht_entry->head_list;
        ht_entry->head_list = new_item;
        new_item->hash_table_next = old_head;
    }

    printk("%s: Inserimento blocco %lld nella entry #%d.\n", MOD_NAME, index, num_entry);

    insert_sorted_list(new_item);

    asm volatile("mfence");//make it visible to readers

    return 0;   
    
}

void scan_free_list(void)
{
    struct block_free *curr;

    curr = head_free_block_list;

    printk("%s: ------------------------------INIZIO FREE LIST------------------------------------------", MOD_NAME);

    while(curr!=NULL)
    {
        printk("Blocco #%lld\n", curr->block_index);
        curr = curr->next;
    }

    printk("%s: ----------------------------FINE FREE LIST-------------------------------------------", MOD_NAME);
}

void scan_sorted_list(void)
{
    struct block *curr;

    curr = head_sorted_list;
    
    printk("%s: ----------------------------------INIZIO SORTED LIST  ---------------------------------------------", MOD_NAME);

    while(curr!=NULL)
    {
        printk("Blocco #%lld - Messaggio %s\n", curr->block_index, curr->msg);
        curr = curr->sorted_list_next;
    }

    printk("%s: --------------------------------FINE SORTED LIST -----------------------------------------", MOD_NAME);
    
}



void scan_hash_table(int x)
{
    int entry_num;
    struct ht_valid_entry entry;
    struct block *item;
    
    printk("%s:-------------------------- INIZIO HASH TABLE --------------------------------------------------\n", MOD_NAME);

    for(entry_num=0; entry_num<x; entry_num++)
    {
        printk("%s: ---------------------------------------------------------------------------------", MOD_NAME);
        entry = hash_table_valid[entry_num];
        item = entry.head_list;

        while(item!=NULL)
        {
            printk("%s: Blocco #%lld\n", MOD_NAME, item->block_index);
            item = item ->hash_table_next;
        }

        printk("%s: ---------------------------------------------------------------------------------", MOD_NAME);
        
    }

    printk("%s: -------------------------- FINE HASH TABLE --------------------------------------------------\n", MOD_NAME);
}



/*
 * Inizializza le tre strutture dati core del modulo.
 */
static int init_data_structure_core(uint64_t num_data_block)
{
    int x;
    uint64_t index;
    size_t size;
    struct buffer_head *bh = NULL;
    struct soafs_block *data_block = NULL;

    if(sb_global == NULL)
    {
        printk("%s: Il contenuto del superblocco non è valido. Impossibile inizializzare le strutture dati core.\n", MOD_NAME);
        return 1;
    }

    if(num_data_block <= 0)
    {
        printk("%s: Il numero di blocchi nel device non è valido. Impossibile inizializzare le strutture dati core.\n", MOD_NAME);
        return 1;
    }

    x = compute_num_rows(num_data_block);                   /* Computo il numero delle entry X della tabella hash */

    size = x * sizeof(struct ht_valid_entry);                   /* Computo la quantità di memoria da allocare */

    hash_table_valid = (struct ht_valid_entry *)kmalloc(size, GFP_KERNEL);

    if(hash_table_valid==NULL)
    {
        printk("%s: Errore malloc.", MOD_NAME);
        return 1;
    } 

    printk("%s: Il numero di liste nella tabella hash è %d\n", MOD_NAME, x);

    for(index=0;index<x;index++)
    {
        hash_table_valid[index].head_list = NULL;
    }

    /*
     * Eseguo la lettura dei blocchi dal device per
     * inizializzare le strutture dati. Se il blocco
     * su cui sto iterando ha un contenuto valido
     * allora dovrò aggiungere le relative informazioni
     * alle strutture dati hash_table_valid e head_sorted_list;
     * altrimenti, dovrò aggiungere le informazioni sul blocco
     * all'interno della lista head_free_block_list.
     */
    for(index=0; index<num_data_block; index++)
    {

        /* Leggo il blocco dal device */
        bh = sb_bread(sb_global, NUM_NODATA_BLOCK + index);                   

        if(bh == NULL)
        {
            printk("%s: Errore buffer head...\n", MOD_NAME);
            return 1;
        }

        data_block = (struct soafs_block *)bh->b_data;

        if(data_block->metadata & MASK_VALID)
        {
            // printk("%s: Il blocco di dati con indice %lld è valido e si trova in posizione %lld.\n", MOD_NAME, index, data_block->metadata & MASK_POS);
            insert_hash_table_valid(data_block, data_block->metadata & MASK_POS, index, x);
        }
        else
        {
            // printk("%s: Il blocco di dati con indice %lld non è valido e si trova in posizione %lld.\n", MOD_NAME, index, data_block->metadata & MASK_POS);
            insert_free_list(index);
        }

        brelse(bh);
    }

    scan_free_list();

    scan_sorted_list();

    scan_hash_table(x);

    return 0;
}



static int soafs_fill_super(struct super_block *sb, void *data, int silent) {   

    struct inode *root_inode;
    struct dentry *root_dentry;
    struct soafs_super_block *sb_disk;
    struct timespec64 curr_time;
    struct buffer_head *bh;
    uint64_t num_block = -1;
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

    /* Faccio il check per verificare se il numero di blocchi nel dispositivo è valido */
    num_block = sb_disk->num_block;

    if(num_block > NBLOCKS)
    {
        printk("%s: Il numero di blocchi del dispositivo non è valido.\n", MOD_NAME);
        brelse(bh);
        return -EINVAL;
    }

    printk("%s: Il numero di blocchi del dispositivo è pari a %lld.\n", MOD_NAME, num_block);

    /* Verifico il valore del magic number presente nel superblocco sul device. */
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

    sb_global = sb;

    /* 
     * Devo escludere i due blocchi contenenti rispettivamente
     * il superblocco e l'inode del file.
     */
    ret = init_data_structure_core(num_block - 2);

    if(ret)
    {
        printk("%s: Errore nella inizializzazione delle strutture dati core del modulo.\n", MOD_NAME);
        return -ENOMEM;
    }

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


