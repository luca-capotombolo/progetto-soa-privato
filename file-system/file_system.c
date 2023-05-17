#include <linux/fs.h>           /* sb_set_blocksize()-iget_locked()-inode_init_owner()-unlock_new_inode()-kill_block_super()-mount_bdev() */
#include <linux/timekeeping.h>
#include <linux/time.h>         /* ktime_get_real_ts64() */
#include <linux/buffer_head.h>  /* sb_bread()-brelse() */
#include <linux/dcache.h>       /* d_make_root() */
#include <linux/string.h>       /* strlen() */
#include <linux/log2.h>         /* ilog2()  */
#include <linux/slab.h>         /* kmalloc() */
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/version.h>	/* For LINUX_VERSION_CODE */

#include "../headers/main_header.h"



int is_mounted = 0;                                             /* Inizialmente non ho alcun montaggio. */
int stop = 1;                                                   
uint64_t num_threads_run = 0;                                   /* Numero di threads attualmente in esecuzione */

struct super_block *sb_global = NULL;                           /* Riferimento al superblocco. */
int is_free = 0;
int kernel_thread_ok = 0;


static struct super_operations soafs_super_ops = {

};



static struct dentry_operations soafs_dentry_ops = {

};



/*
 * Questa funzione ha il compito di determinare
 * i primi blocchi liberi nel device. Il numero
 * massimo di blocchi liberi che può essere caricato
 * all'interno dell'array è pari a update_list_size.
 * Inoltre, la funzione determina il numero totale di
 * blocchi liberi nel device.
 */
static int set_free_block(void)
{
    struct soafs_sb_info *sbi;
    struct buffer_head *bh;
    struct soafs_super_block * b;
    uint64_t *free_blocks;
    uint64_t index;
    uint64_t num_block_data;
    uint64_t counter;
    uint64_t dim;
    uint64_t update_list_size;
    int ret;
    
    sbi = (struct soafs_sb_info *)sb_global->s_fs_info;

    update_list_size = sbi->update_list_size;
    
    free_blocks = (uint64_t *)kzalloc(sizeof(uint64_t) * update_list_size, GFP_KERNEL);

    if(free_blocks == NULL)
    {
        printk("%s: [ERRORE SMONTAGGIO - SET FREE BLOCK] Errore esecuzione della kzalloc() nella ricerca dei blocchi liberi\n", MOD_NAME);
        return 1;
    }

    printk("%s: [SMONTAGGIO - SET FREE BLOCK] Inizio ricerca blocchi liberi (al massimo %lld)...\n", MOD_NAME, update_list_size);

    bh = sb_bread(sb_global, SOAFS_SB_BLOCK_NUMBER);    

    if(bh == NULL)
    {
        printk("%s: [ERRORE FREE BLOCK] La lettura del superblocco è terminata senza successo\n", MOD_NAME);
        kfree(free_blocks);
        return 1;
    }

    b = (struct soafs_super_block *)bh->b_data;

    num_block_data = sbi->num_block - 2 - sbi->num_block_state;

    if((num_block_free_used == sbi->num_block_free) && (head_free_block_list == NULL))
    {
        printk("%s: [SMONTAGGIO - SET FREE BLOCK] I blocchi liberi a disposizione sono terminati\n", MOD_NAME);

        b->actual_size = 0;

        b->num_block_free = 0;

        mark_buffer_dirty(bh);

        sync_dirty_buffer(bh);
    
        brelse(bh);

        kfree(free_blocks);

        return 0;
    }

    /*
     * Rappresenta il numero attuale di blocchi
     * liberi che sono stati identificati. Di questi
     * blocchi, al più update_list_size verranno
     * inseriti all'interno dell'array.
     */    
    counter = 0;
    dim = 0;

    for(index = 0; index < num_block_data; index++)
    {
        ret = check_bit(index);

        if(!ret)
        {
#ifdef NOT_CRITICAL_INIT
            printk("%s: [SMONTAGGIO - SET FREE BLOCK] Blocco libero #%lld\n", MOD_NAME, index);
#endif
            if(counter < update_list_size)
            {
                free_blocks[counter] = index;
                dim++;
            }          

            counter++;
        }

    }

    b->num_block_free = counter;

    printk("%s: [SMONTAGGIO - SET FREE BLOCK] Numero di blocchi liberi trovati %lld\n", MOD_NAME, counter);

    b->actual_size = dim;

    printk("%s: [SMONTAGGIO - SET FREE BLOCK] Dimensione array %lld\n", MOD_NAME, dim);
    
    for(index = 0; index < dim; index++)
    {
        b-> index_free[index] = free_blocks[index];
    }

    mark_buffer_dirty(bh);

    sync_dirty_buffer(bh);
    
    brelse(bh);

    kfree(free_blocks);

    return 0;   
    
}



void wake_up_umount(void)
{
    __sync_fetch_and_sub(&(num_threads_run),1);

    wake_up_interruptible(&umount_queue);
}



/*
 * Questa funzione esegue il flush dei blocchi di stato sul
 * device. In questo modo, al successivo montaggio, sono in
 * grado di stabilire quali sono i blocchi validi.
 */
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
            printk("%s: [ERRORE SMONTAGGIO - FLUSH BITMASK] Errore nella lettura del blocco di stato #%lld\n", MOD_NAME, counter);
            return 1;
        }

        b = (uint64_t *)bh->b_data;

        for(i = 0; i < 512; i++)
        {
            b[i] = bitmask[counter][i];
        }

        mark_buffer_dirty(bh);

        sync_dirty_buffer(bh);

#ifdef NOT_CRITICAL_INIT
        printk("%s: [SMONTAGGIO - FLUSH BITMASK] Flush dei dati per il blocco di stato #%lld avvenuto con successo\n", MOD_NAME, counter);        
#endif
        counter++;

        brelse(bh);
    }

    printk("%s: [SMONTAGGIO - FLUSH BITMASK] Blocchi di stato flushati correttamente: %lld/%lld\n", MOD_NAME, counter, num_block_state);

    return 0;
}



/*
 * Questa funzione ha il compito di riportare i messaggi utente
 * validi all'interno dei corrispettivi blocchi del device.
 */
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
        printk("%s: [SMONTAGGIO - FLUSH VALID BLOCK] Non ci sono messaggi validi da riportare\n", MOD_NAME);
        return 0;
    }

    sbi = (struct soafs_sb_info *)sb_global->s_fs_info;

    num_block_state = sbi->num_block_state;
    
    pos = 0;
    
    while(curr != NULL)
    {
        index = curr->block_index;
#ifdef NOT_CRITICAL_INIT
        printk("%s: [SMONTAGGIO - FLUSH VALID BLOCK]  Il blocco con indice %lld deve essere riportato su device\n", MOD_NAME, index);
#endif
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

    printk("%s: [SMONTAGGIO - FLUSH VALID BLOCK] I blocchi validi sono stati riportati correttamente su device\n", MOD_NAME);
    
    return 0;
}



/*
 * Questa funzione ha il compito di deallocare le strutture
 * dati core che sono state utilizzate per l'attuale istanza
 * di montaggio del FS.
 * Una nuova istanza di montaggio del FS necessiterà di nuove
 * strutture dati.
 */
void free_all_memory(void)
{
    struct block_free *next_bf;
    struct block *next_b;
    struct soafs_sb_info *sbi;
    uint64_t num_block_state;
    int index;

    /* Deallocazione degli elementi nella lista dei blocchi liberi */
    if(head_free_block_list!=NULL)
    {
        printk("%s: [SMONTAGGIO - FREE MEMORY] La lista dei blocchi liberi non è vuota... inizio deallocazione in corso...\n", MOD_NAME);

        while(head_free_block_list!=NULL)
        {
            next_bf = head_free_block_list->next;

#ifdef NOT_CRITICAL_INIT
            printk("%s: [SMONTAGGIO - FREE MEMORY] Deallocazione blocco #%lld...\n", MOD_NAME, head_free_block_list->block_index);
#endif

            kfree(head_free_block_list);

#ifdef NOT_CRITICAL_INIT
            printk("%s: [SMONTAGGIO - FREE MEMORY] Deallocazione blocco #%lld avvenuta con successo\n", MOD_NAME, head_free_block_list->block_index);
#endif

            head_free_block_list = next_bf;
        }

        printk("%s: [SMONTAGGIO - FREE MEMORY] Deallocazione dei blocchi dalla lista libera completata con successo\n", MOD_NAME);        
    }

    /* Setto a NULL la testa delle liste nella HT */
    for(index=0;index<x;index++)
    {
        (&hash_table_valid[index])->head_list = NULL;
    }

    /* Dealloco gli elementi che rappresentano i blocchi validi */
    if(head_sorted_list != NULL)
    {

        printk("%s: [SMONTAGGIO - FREE MEMORY] La lista dei blocchi ordinati non è vuota... inizio deallocazione in corso...\n", MOD_NAME);

        while(head_sorted_list != NULL)
        {
            next_b = head_sorted_list->sorted_list_next;

#ifdef NOT_CRITICAL_INIT
            printk("%s: [SMONTAGGIO - FREE MEMORY] Deallocazione blocco #%lld...\n", MOD_NAME, head_sorted_list->block_index);
#endif

            kfree(head_sorted_list);

#ifdef NOT_CRITICAL_INIT
            printk("%s: [SMONTAGGIO - FREE MEMORY] Deallocazione blocco #%lld avvenuta con successo\n", MOD_NAME, head_sorted_list->block_index);
#endif

            head_sorted_list = next_b;
        }
    
        printk("%s: [SMONTAGGIO - FREE MEMORY] Deallocazione dei blocchi dalla lista ordinata completata con successo\n", MOD_NAME);
    }

    /* Dealloco la tabella hash */
    kfree(hash_table_valid);

    hash_table_valid = NULL;
    
    printk("%s: [SMONTAGGIO - FREE MEMORY] Deallocazione della HT avvenuta con successo\n", MOD_NAME);

    /* Dealloco la maschera di bit */
    sbi = (struct soafs_sb_info *)sb_global->s_fs_info;

    num_block_state = sbi->num_block_state;

    for(index=0; index<num_block_state; index++)
    {
        kfree(bitmask[index]);
    }

    kfree(bitmask);

    bitmask = NULL;

    printk("%s: [SMONTAGGIO - FREE MEMORY] Deallocazione della BITMASK avvenuta con successo\n", MOD_NAME);

    if(head_free_block_list != NULL)
    {
        printk("%s: [ERRORE SMONTAGGIO - FREE MEMORY] La lista dei blocchi liberi non è vuota\n", MOD_NAME);
    }

    if(head_sorted_list != NULL)
    {
        printk("%s: [ERRORE SMONTAGGIO - FREE MEMORY] La lista dei blocchi ordinata non è vuota\n", MOD_NAME);
    }

}



int house_keeper(void *arg)
{
    int index_ht;
    int index_sorted;
    unsigned long updated_epoch_ht;
    unsigned long updated_epoch_sorted;
    unsigned long last_epoch_ht;
    unsigned long last_epoch_sorted;
    unsigned long grace_period_threads_ht;
    unsigned long grace_period_threads_sorted;
    uint64_t num_insert;

sleep_kt:

    msleep(PERIOD * 1000);

    mutex_lock(&invalidate_mutex);

retry_house_keeper:

    /* Sezione critica tra inserimenti e invalidazioni */
    mutex_lock(&inval_insert_mutex);

    /* Recupero il numero di inserimenti in corso */
    num_insert = sync_var & MASK_NUMINSERT;

    printk("%s: [HOUSE KEEPER] Il numero di inserimenti attualmente in corso è pari a %lld\n", MOD_NAME, num_insert);

    /*
     * Verifico se esistono degli inserimenti o un'invalidazione
     * in corso. Se non esistono inserimenti in corso, allora si
     * necessita solamente di verificare se esiste una invalidazione.
     * Se esiste un'invalidazione, allora la sync_var è differente da
     * zero.
     */
    if(num_insert > 0)
    {
        mutex_unlock(&inval_insert_mutex);

#ifdef NOT_CRITICAL_HK
        printk("%s: [HOUSE KEEPER] Il thread demone viene messo in attesa\n", MOD_NAME);
#endif

        wait_event_interruptible_timeout(the_queue, (sync_var & MASK_NUMINSERT) == 0, msecs_to_jiffies(100));

#ifdef NOT_CRITICAL_HK
        printk("%s: [HOUSE KEEPER] Il thread demone riprende l'esecuzione\n", MOD_NAME);
#endif
        goto retry_house_keeper;
    }

    if(sync_var)
    {
        mutex_unlock(&inval_insert_mutex);

        mutex_unlock(&invalidate_mutex);

        goto sleep_kt;
    }

    /* Comunico l'inizio del processo di azzeramento del contatore */
    __atomic_exchange_n (&(sync_var), 0X8000000000000000, __ATOMIC_SEQ_CST);

    mutex_unlock(&inval_insert_mutex);

    /*
     * Durante il processo di invalidazione non è possibile
     * avere in concorrenza l'inserimento di un nuovo blocco.
     */
    updated_epoch_ht = (gp->next_epoch_index_ht) ? MASK : 0;
    updated_epoch_sorted = (gp->next_epoch_index_sorted) ? MASK : 0;

    gp->next_epoch_index_ht += 1;
    gp->next_epoch_index_ht %= 2;
    gp->next_epoch_index_sorted += 1;
    gp->next_epoch_index_sorted %= 2;

    last_epoch_ht = __atomic_exchange_n (&(gp->epoch_ht), updated_epoch_ht, __ATOMIC_SEQ_CST);
    last_epoch_sorted = __atomic_exchange_n (&(gp->epoch_sorted), updated_epoch_sorted, __ATOMIC_SEQ_CST);

    index_ht = (last_epoch_ht & MASK) ? 1:0;
    index_sorted = (last_epoch_sorted & MASK) ? 1:0;

    grace_period_threads_ht = last_epoch_ht & (~MASK);
    grace_period_threads_sorted = last_epoch_sorted & (~MASK);

#ifdef NOT_CRITICAL_HK
    printk("%s: [HOUSE KEEPER] Attesa della terminazione del grace period HT: #threads %ld\n", MOD_NAME, grace_period_threads_ht);
    printk("%s: [HOUSE KEEPER] Attesa della terminazione del grace period lista ordinata: #threads %ld\n", MOD_NAME, grace_period_threads_sorted);
#endif

sleep_again:

    wait_event_interruptible_timeout(the_queue, (gp->standing_ht[index_ht] >= grace_period_threads_ht) && (gp->standing_sorted[index_sorted] >= grace_period_threads_sorted), msecs_to_jiffies(100));

#ifdef NOT_CRITICAL_BUT_HK
    printk("%s: gp->standing_ht[index_ht] = %ld\tgrace_period_threads_ht = %ld\tgp->standing_sorted[index_sorted] = %ld\tgrace_period_threads_sorted = %ld\n", MOD_NAME, gp->standing_ht[index_ht], grace_period_threads_ht, gp->standing_sorted[index_sorted], grace_period_threads_sorted);
#endif

    if((gp->standing_ht[index_ht] < grace_period_threads_ht) || (gp->standing_sorted[index_sorted] < grace_period_threads_sorted))
    {
        printk("%s: [ERRORE HOUSE KEEPER] Il thread demone va nuovamente a dormire\n", MOD_NAME);
        goto sleep_again;
    }

    gp->standing_sorted[index_sorted] = 0;

    gp->standing_ht[index_ht] = 0;

    __atomic_exchange_n (&(sync_var), 0X0000000000000000, __ATOMIC_SEQ_CST);

    mutex_unlock(&invalidate_mutex);

    wake_up_interruptible(&the_queue);

    goto sleep_kt;
    
}


/*
 * Questa funzione ha il compito di creare il thread demone che
 * si occuperà di resettare il valore del contatore in modo da
 * evitare l'overflow.
 */
int new_thread_daemon(void)
{
    struct task_struct * kt;
    int ret;

    kt = kthread_create(house_keeper, NULL, "house_keeper");

	if (IS_ERR(kt)) {

		printk("%s: [ERRORE DEMONE] Errore creazione kernel thread\n", MOD_NAME);

        return 1;
	}

    printk("%s: [DEMONE] Creazione del thread demone avvenuta con successo\n", MOD_NAME);

    ret = wake_up_process(kt);  

    return 0;
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
        printk("%s: [ERRORE MONTAGGIO] Errore nel setup della dimensione del superblocco.\n", MOD_NAME);

        is_free = 1;

        return -EIO;
    }
   
    /*
     * Il Superblocco è il primo blocco memorizzato sul device.
     * Di conseguenza, l'indice che deve essere utilizzato con la
     * sb_bread ha valore pari a zero.
     */
    bh = sb_bread(sb, SOAFS_SB_BLOCK_NUMBER);

    if(bh == NULL){
        printk("%s: [ERRORE MONTAGGIO] Errore nella lettura del superblocco.\n", MOD_NAME);

        is_free = 1;

	    return -EIO;
    }

    /* Recupero il superblocco memorizzato sul device. */
    sb_disk = (struct soafs_super_block *)bh->b_data;  

    /* Faccio il check per verificare se il numero di blocchi nel dispositivo è valido. */
    if( (sb_disk->num_block > NBLOCKS) || ((sb_disk->num_block - sb_disk->num_block_state - 2) <= 0) )
    {
        printk("%s: [ERRORE MONTAGGIO] Il numero di blocchi del dispositivo non è valido.\n", MOD_NAME);

        brelse(bh);

        is_free = 1;

        return -EINVAL;
    }

    /* Verifico il valore del magic number presente nel superblocco sul device. */
    if(sb_disk->magic != SOAFS_MAGIC_NUMBER)
    {
        printk("%s: [ERRORE MONTAGGIO] Mancata corrispondenza tra i due magic number.\n", MOD_NAME);

        brelse(bh);

        is_free = 1;

	    return -EBADF;
    }

    /* Popolo la struttura dati del superblocco generico. */
    sb->s_magic = SOAFS_MAGIC_NUMBER;
    sb->s_op = &soafs_super_ops;

    /* Recupero informazioni FS specific. */
    sbi = (struct soafs_sb_info *)kzalloc(sizeof(struct soafs_sb_info), GFP_KERNEL);

    if(sbi == NULL)
    {
        printk("%s: [ERRORE MONTAGGIO] Errore kzalloc() nell'allocazione della struttura dati soafs_sb_info.\n", MOD_NAME);

        brelse(bh);

        is_free = 1;

        return -ENOMEM;
    }

    sbi->num_block = sb_disk->num_block;
    sbi->num_block_free = sb_disk->num_block_free;
    sbi->num_block_state = sb_disk->num_block_state;
    sbi->update_list_size = sb_disk->update_list_size;

    sb->s_fs_info = sbi;

    printk("%s: [MONTAGGIO] Il numero di blocchi del dispositivo è pari a %lld.\n", MOD_NAME, sb_disk->num_block);
    printk("%s: [MONTAGGIO] Il numero di blocchi liberi del dispositivo è pari a %lld.\n", MOD_NAME, sb_disk->num_block_free);
    printk("%s: [MONTAGGIO] Il numero di blocchi di stato del dispositivo è pari a %lld.\n", MOD_NAME, sb_disk->num_block_state);
    printk("%s: [MONTAGGIO] Il numero di blocchi massimo da caricare all'aggiornamento è pari a %lld.\n", MOD_NAME, sb_disk->update_list_size);
    
    /* Recupero il root inode. */
    root_inode = iget_locked(sb, 0);

    if(!root_inode)
    {
        printk("%s: [ERRORE MONTAGGIO] Errore nel recupero del root inode.\n", MOD_NAME);

        brelse(bh);

        kfree(sbi);

        is_free = 1;

        return -ENOMEM;
    }

    root_inode->i_ino = SOAFS_ROOT_INODE_NUMBER;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 12, 0)
    inode_init_owner(&init_user_ns, root_inode, NULL, S_IFDIR);
#else
    inode_init_owner(root_inode, NULL, S_IFDIR);
#endif

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
        printk("%s: [ERRORE MONTAGGIO] Errore nella creazione della root directory.\n", MOD_NAME);

        brelse(bh);

        kfree(sbi);

        is_free = 1;

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
        printk("%s: [ERRORE MONTAGGIO] Errore nella inizializzazione delle strutture dati core del modulo.\n", MOD_NAME);

        brelse(bh);

        kfree(sbi);

        return -EIO;
    }

    ret = __sync_bool_compare_and_swap(&kernel_thread_ok, 0, 1);

    if(!ret)
    {
        printk("%s: [ERRORE MONTAGGIO] Il thread demone esiste già...\n", MOD_NAME);
        goto exit_mont;
    }

    ret = new_thread_daemon();

    if(ret)
    {
        printk("%s: [ERRORE MONTAGGIO] Errore nella creazione del thread demone\n", MOD_NAME);

        brelse(bh);

        free_all_memory();

        kfree(sbi);

        is_free = 1;

        return -EIO;        
    }

exit_mont:

    /* Rilascio del superblocco */
    brelse(bh);

    return 0;
}



static void soafs_kill_sb(struct super_block *sb)
{

    int ret;
    int n;

    n = 0;

    printk("%s: [SMONTAGGIO] Inizio smontaggio del FS...", MOD_NAME);

    if(is_free)
    {
        printk("%s: [SMONTAGGIO] Le strutture dati sono state già deallocate e il FS non è stato utilizzato\n", MOD_NAME);
        goto exit_kill_sb;
    }

    ret = __sync_bool_compare_and_swap(&(stop), 0, 1);

    if(!ret)
    {
        return ;
    }

    ret = __sync_bool_compare_and_swap(&(is_mounted), 1, 0);

    if(!ret)
    {
        printk("%s: [ERRORE SMONTAGGIO] Il valore della variabile is_mounted non è corretto\n", MOD_NAME);
        return ;
    }

umount_retry_running:

    if(num_threads_run > 0)
    {
        wait_event_interruptible_timeout(umount_queue, num_threads_run == 0, msecs_to_jiffies(100));

        n++;

        goto umount_retry_running;
    }

    printk("%s: Il numero di tentativi richiesto per lo smontaggio è pari a %d\n", MOD_NAME, n);

    n = 0;

retry_flush_bitmask:

    ret = flush_bitmask();

    if(ret)
    {
        n++;
        printk("%s: [ERRORE SMONTAGGIO] Tentativo numero %d fallito per il flush della bitmask\n", MOD_NAME, n);
        goto retry_flush_bitmask;
    }

    printk("%s: Flush della bitmask eseguito con successo\n", MOD_NAME);

    n = 0;

retry_set_free_block:

    ret = set_free_block();

    if(ret)
    {
        n++;
        printk("%s: [ERRORE SMONTAGGIO] Tentativo numero %d fallito per il settaggio dei blocchi liberi\n", MOD_NAME, n);
        goto retry_set_free_block;
    }

    printk("%s: Settaggio dei blocchi liberi eseguito con successo\n", MOD_NAME);

    n = 0;

retry_umount:

    ret = flush_valid_block();

    if(ret)
    {
        n++;
        printk("%s: [ERRORE SMONTAGGIO] Tentativo numero %d fallito per il salvataggio dei blocchi validi\n", MOD_NAME, n);
        goto retry_umount;
    }

    printk("%s: Flush dei blocchi validi eseguito con successo\n", MOD_NAME);

    free_all_memory();

exit_kill_sb:

    kill_block_super(sb);

    is_free = 0;

    sync_var = 0;

    printk("%s: [SMONTAGGIO] Il File System 'soafs' è stato smontato con successo.\n", MOD_NAME);
}



static struct dentry *soafs_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data)
{
    int ret_cmp;
    struct dentry *ret;

    ret_cmp = __sync_bool_compare_and_swap(&is_mounted, 0, 1);

    if(!ret_cmp)
    {
        printk("%s: [ERRORE MONTAGGIO] Esiste già un altro montaggio del file system di tipo %s\n", MOD_NAME, fs_type->name);
        return ERR_PTR(-EINVAL);
    }
    
    ret = mount_bdev(fs_type, flags, dev_name, data, soafs_fill_super);         /* Monta un file system memorizzato su un block device */

    if (unlikely(IS_ERR(ret)))
    {
        printk("%s: [ERRORE MONTAGGIO] Errore durante il montaggio del File System 'soafs'.\n",MOD_NAME);

        __sync_fetch_and_sub(&is_mounted,1);
    }
    else
    {

        printk("%s: Numero di thread in esecuzione pari a %lld\n", MOD_NAME, num_threads_run);

        ret_cmp = __sync_bool_compare_and_swap(&stop, 1, 0);

        if(!ret_cmp)
        {
            printk("%s: [ERRORE MONTAGGIO] La barriera per lo stop dello smontaggio non è settata correttamente\n", MOD_NAME);
            return ERR_PTR(-EINVAL);
        }  

        printk("%s: [MONTAGGIO] Montaggio del File System sul device %s avvenuto con successo.\n",MOD_NAME,dev_name);
    }

    return ret;
}



struct file_system_type soafs_fs_type = {
	.owner          = THIS_MODULE,
    .name           = "soafs",
    .mount          = soafs_mount,
    .kill_sb        = soafs_kill_sb,
};


