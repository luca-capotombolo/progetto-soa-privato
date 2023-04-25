#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>	        /* Needed for KERN_INFO */
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/syscalls.h>
#include <linux/slab.h>             /* kmalloc() */
#include <linux/uaccess.h>          /* copy_from_user() */
#include <linux/buffer_head.h>      /* sb_bread()       */
#include <linux/log2.h>             /* ilog2()  */
#include "lib/include/scth.h"
#include <linux/string.h>       /* strncpy() */
#include "./headers/main_header.h"



MODULE_LICENSE(LICENSE);
MODULE_AUTHOR(AUTHOR);
MODULE_DESCRIPTION(DESCRIPTION);



unsigned long the_syscall_table = 0x0;
module_param(the_syscall_table, ulong, 0660);
unsigned long the_ni_syscall;
unsigned long new_sys_call_array[] = {0x0, 0x0, 0x0};
int restore[HACKED_ENTRIES] = {[0 ... (HACKED_ENTRIES-1)] -1};

/*
 * Questo mutex mi consente di gestire il caricamento
 * delle informazioni relative ai blocchi liberi
 * all'interno della lista una sola volta.
 */
static DEFINE_MUTEX(free_list_mutex);



static int check_offset(int offset, struct soafs_sb_info *sbi)
{
    /*
     * Tengo conto dei primi due blocchi, contenenti rispettivamente
     * il superblocco e l'inode del file, dei blocchi di stato e del
     * fatto che il primo blocco ha indice pari a 0 (e non 1) e quindi
     * l'offset può assumere valore 0.
     */
    if( ( (2 + sbi->num_block_state + offset) > (sbi->num_block - 1) ) || (offset < 0) )
    {
        return 1;
    }

    return 0;
}



static int check_size(size_t size)
{

    if(size <= 0)
    {
        return 1;
    }

    return 0;
}



#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(3, _get_data, uint64_t, offset, char *, destination, size_t, size){
#else
asmlinkage int sys_get_data(uint64_t offset, char * destination, size_t size){
#endif

    struct soafs_sb_info *sbi;
    struct ht_valid_entry *entry;
    int ret;
    int i;
    int available_data;
    size_t byte_ret;
    size_t byte_copy;                                           /* Il numero di byte che verranno restituiti all'utente */
    char *msg_block;                                            /* Il messaggio contenuto all'interno del blocco del device */   
    unsigned long my_epoch;
    int index;
    
    LOG_SYSTEM_CALL("GET_DATA", "get_data");
    
    ret = check_is_mounted();                                   /* verifico se il file system su cui si deve operare è stato effettivamente montato */

    if(!ret)
    {
        LOG_DEV_ERR("GET_DATA", "get_data");
        return -ENODEV;
    }

    sbi = (struct soafs_sb_info *)sb_global->s_fs_info;

    /* Faccio un controllo sui parametri */
    if( check_offset(offset, sbi) || check_size(size) )
    {
        LOG_PARAM_ERR("GET_DATA", "get_data");
        return -EINVAL;
    }

    i = offset % x;                                             /* Determino la lista che contiene il blocco */

    entry = &(hash_table_valid[i]);

    my_epoch = __sync_fetch_and_add(&(gp->epoch_ht),1);

    /* Verifico se il blocco richiesto è valido */
    if(!check_bit(offset))
    {
        printk("%s: [ERRORE GET DATA] E' stata richiesta la lettura del blocco %lld ma il blocco non è valido\n", MOD_NAME, offset);

        index = (my_epoch & MASK) ? 1 : 0;

        __sync_fetch_and_add(&(gp->standing_ht[index]),1);

        wake_up_interruptible(&the_queue);

        return -ENODATA;
    }

    msg_block = read_data_block(offset, entry);

	index = (my_epoch & MASK) ? 1 : 0;

	__sync_fetch_and_add(&(gp->standing_ht[index]),1);

    wake_up_interruptible(&the_queue);  

    if(msg_block == NULL)
    {
        printk("%s: [ERRORE GET DATA] La lettura del blocco %lld è terminata con insuccesso\n", MOD_NAME, offset);
        return -ENODATA;
    }

    available_data = strlen(msg_block) + 1;

    if(size > available_data)
    {
        /*
        * Se la quantità di dati che mi stai chiedendo
        * è strettamente maggiore della dimensione del
        * messaggio che deve essere restituit, allora ti
        * restituisco l'intero contenuto del messaggio. Il
        * terminatore di stringa è già presente nel blocco.
        */
        byte_copy = available_data;
    }
    else
    {        
        /*
         * Altrimenti, ti restituisco la quantità di byte che
         * mi chiedi. Metto il terminatore di stringa poiché
         * il messaggio contenuto nel blocco potrebbe essere
         * più lungo della dimensione 'size' richiesta.
         */
        byte_copy = size;

        if(size < available_data)
        {
            msg_block[byte_copy-1] = '\0';
        }
      
    }    

    //printk("%s: [GET DATA] Numero di bytes che verranno effettivamente restituiti è %ld\n", MOD_NAME, byte_copy);
    
    byte_ret = copy_to_user(destination, msg_block, byte_copy);

    printk("%s: [GET DATA] Il messaggio del blocco %lld è stato consegnato con successo\n", MOD_NAME, offset);
    
    return (byte_copy - byte_ret);
	
}



#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(2, _put_data, char *, source, size_t, size){
#else
asmlinkage uint64_t sys_put_data(char * source, size_t size){
#endif
    struct soafs_sb_info *sbi;
    struct block_free *item;
    int available_data;
    int ret;
    int n;                          /* Numero corrente di tentativi per il recupero del blocco libero */
    char *msg;                      /* Il messaggio che si richiede di scrivere nel blocco */
    size_t msg_size;                /* La dimensione del messaggio che verrà scritto nel blocco */    
    size_t bytes_to_copy;           /* Il numero di bytes che verranno effettivamente copiati dallo spazio utente */    
    unsigned long bytes_ret;        /* Il numero di bytes effettivamente copiati */
    uint64_t index;

    LOG_SYSTEM_CALL("PUT_DATA", "put_data");

    ret = check_is_mounted();   /* verifico se il file system su cui si deve operare è stato effettivamente montato */

    if(!ret)
    {
        LOG_DEV_ERR("PUT_DATA", "put_data");
        return -ENODEV;
    }

    if(check_size(size))
    {
        LOG_PARAM_ERR("PUT_DATA", "put_data");
        return -EINVAL;
    }

    sbi = (struct soafs_sb_info *)sb_global->s_fs_info;

    /*
     * Verifico se vale la pena eseguire una ricerca dei blocchi
     * liberi oppure sono già tutti occupati.
     */
    if( (head_free_block_list == NULL) && (num_block_free_used == sbi->num_block_free) )
    {
        printk("%s: [ERRORE PUT DATA] Non ci sono più blocchi liberi da utilizzare\n", MOD_NAME);
        return -ENOMEM;
    }

    item = NULL;

    /*
     * Recupero l'indice del blocco libero da utilizzare per inserire
     * il messaggio dell'utente. Se la lista dei blocchi liberi è vuota,
     * allora devo recuperare le informazioni relative ad altri blocchi
     * liberi; altrimenti, estraggo il blocco in testa alla lista.
     */
    if(head_free_block_list == NULL)
    {
    
        /*
         * Devo trovare nuovi blocchi liberi tra quelli
         * che erano liberi al tempo di montaggio e che
         * finora non ho mai utilizzato (i.e., il cui
         * indice non è stato mai caricato nella lista).
         */

        n = 0;

retry:

        if(n > 5)
        {
            printk("%s: [ERRORE PUT DATA] Numero di tentativi esaurito per il recupero di un blocco libero.\n", MOD_NAME);
            return -ENOMEM;
        }

        /* Prendo il mutex per caricare i blocchi nella lista una sola volta */
        mutex_lock(&free_list_mutex);

        /* Inserisco i nuovi blocchi liberi all'interno della lista. */
        ret = get_bitmask_block();

        /* Rilascio il mutex per terminare la sezione critica */
        mutex_unlock(&free_list_mutex);

        if(ret)
        {
                printk("%s: [ERRORE PUT DATA] Errore esecuzione kmalloc() nel recupero di un blocco libero\n", MOD_NAME);
                return -EIO;    
        }

        /*
         * Per via della concorrenza, è possibile che la testa
         * della lista risulti essere NULL.
         * In questo caso, non esistono blocchi liberi da utilizzare.
         */
        if( (head_free_block_list == NULL) && (num_block_free_used == sbi->num_block_free) )
        {
                printk("%s: [ERRORE PUT DATA] Non ci sono più blocchi liberi da utilizzare\n", MOD_NAME);
                return -ENOMEM;
        }

        /*
         * In questo caso, la lista si è nuovamente svuotata. Si tenta
         * di caricare nuovamente i blocchi liberi nella lista.
         */
        if( (head_free_block_list == NULL) && (num_block_free_used < sbi->num_block_free))
        {
            printk("%s: [ERRORE PUT DATA] Tentativo #%d fallito per il recupero dei blocchi liberi\n", MOD_NAME, n);
            n++;
            goto retry;
        }

    }

    /* Prendo il blocco in testa alla lista */
    item = get_freelist_head();
    
    if(item == NULL)
    {
        printk("%s: [ERRORE PUT DATA] Errore nel recupero di un blocco libero\n", MOD_NAME);
        return -ENOMEM;
    }

    index = item -> block_index;

    printk("%s: [PUT DATA] Indice del blocco libero da utilizzare - %lld\n", MOD_NAME, index);

    /* Calcolo i byte effettivi del messaggio da scrivere nel blocco */

    available_data = SOAFS_BLOCK_SIZE - (sizeof(uint64_t));

    if(size > available_data)
    {
        /*
         * Si sta chiedendo di scrivere un messaggio
         * che ha una dimensione maggiore della dimensione
         * massima consentita per un messaggio utente.
         * Di conseguenza, il messaggio verrà scritto solo
         * parzialmente. La componente '-1' della variabile
         * 'bytes_to_copy' è dovuta al fatto che bisogna
         * tener conto del terminatore di stringa '\0'.
         */
        msg_size = available_data;
        bytes_to_copy = msg_size - 1;   
     
    }
    else
    {
        /*
         * Si assume che la stringa passata in input
         * contenga già il terminatore di stringa '\0'.
         */
        msg_size = size;
        bytes_to_copy = msg_size;

    }


    /* Alloco la memoria per ospitare il messaggio proveniente dallo spazio utente */
    msg = (char *)kmalloc(msg_size, GFP_KERNEL);

    if(msg == NULL)
    {
        LOG_KMALLOC_ERR("ERRORE PUT DATA");
        return -EIO;    
    }

    bytes_ret = copy_from_user(msg, source, bytes_to_copy);

    printk("[PUT DATA] Numero di bytes non copiati da user space - %ld\n", bytes_ret);

    if(bytes_to_copy!=msg_size)
    {
         /* Inserisco il terminatore di stringa */
        msg[msg_size - 1] = '\0';
    }

    printk("%s: [PUT DATA] E' stato richiesto di scrivere il messaggio '%s'", MOD_NAME, msg);

    ret = insert_hash_table_valid_and_sorted_list_conc(msg, sbi->num_block, index, item);

    if(ret)
    {
        printk("%s: [ERRORE PUT DATA] Errore nell'inserimento del nuovo blocco con indice %lld\n", MOD_NAME, index);
        kfree(msg);
        return -EIO;
    }

    /* Dealloco la struttura dati che rappresenta il blocco libero ottenuto in precedenza */
    kfree(item);

    /* Comunico che il blocco inserito risulta essere valido */
    set_bitmask(index, 1);    

    printk("%s: [PUT DATA] Il messaggio '%s' è stato inserito con successo\n", MOD_NAME, msg);

    return index;


/* ------------------------------------------------------------------------------------------------------ 

    //TODO: determina, se esiste, un blocco libero per la scrittura del messaggio
    bh = sb_bread(sb_global, 2);                            

    if(bh == NULL)
    {
        LOG_BH("put_data", "scrittura", 2, "errore");
        return -EIO;
    }

    LOG_BH("put_data", "scrittura", 2, "successo");  

    msg_block = (char *)bh->b_data;                 

    memcpy(msg_block, msg, msg_size);

    mark_buffer_dirty(bh);                          

    brelse(bh);                                 

    return bytes_to_copy - bytes_ret;*/
	
}



#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(1, _invalidate_data, uint64_t, offset){
#else
asmlinkage int sys_invalidate_data(uint64_t offset){
#endif

    int ret;
    struct soafs_sb_info *sbi;

    LOG_SYSTEM_CALL("INVALIDATE DATA", "invalidate_data");

    ret = check_is_mounted();

    if(!ret)
    {
        LOG_DEV_ERR("INVALIDATE DATA", "invalidate_data");
        return -ENODEV;
    }

    sbi = (struct soafs_sb_info *)sb_global->s_fs_info;

    /* Faccio un controllo sull'indice del blocco passato come parametro */
    if( check_offset(offset, sbi) )
    {
        LOG_PARAM_ERR("INVALIDATE DATA", "invalidate_data");
        return -EINVAL;
    }

    /*
     * Faccio un controllo sulla validità del blocco di cui 
     * si vuole effettuare l'invalidazione.
     */
    if(!check_bit(offset))
    {
        printk("%s: [ERRORE INVALIDATE DATA] E' stata richiesta l'invalidazione del blocco %lld ma il blocco non è valido\n", MOD_NAME, offset);
        return -ENODATA;
    }

    /* Eseguo l'invalidazione del blocco richiesto */
    ret = invalidate_block(offset);

    if(ret)
    {
        printk("%s: [ERRORE INVALIDATE DATA] L'invalidazione del blocco %lld non è stata eseguita con successo\n", MOD_NAME, offset);        
        return -ENODATA;
    }

    printk("%s: [INVALIDATE DATA] L'invalidazione del blocco %lld è stata eseguita con successo\n", MOD_NAME, offset);

    return 0;
	
}



#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
long sys_get_data = (unsigned long) __x64_sys_get_data;
long sys_put_data = (unsigned long) __x64_sys_put_data;
long sys_invalidate_data = (unsigned long) __x64_sys_invalidate_data;       
#else
#endif



static int soafs_init(void)
{

    int ret;
    int i;


	AUDIT
    {
        printk("%s: [MONTAGGIO MODULO] received sys_call_table address %px\n",MOD_NAME,(void*)the_syscall_table);
     	printk("%s: [MONTAGGIO MODULO] initializing - hacked entries %d\n",MOD_NAME,HACKED_ENTRIES);
	}

	new_sys_call_array[0] = (unsigned long)sys_get_data;
    new_sys_call_array[1] = (unsigned long)sys_put_data;
    new_sys_call_array[2] = (unsigned long)sys_invalidate_data;

    ret = get_entries(restore,HACKED_ENTRIES,(unsigned long*)the_syscall_table,&the_ni_syscall);

    if (ret != HACKED_ENTRIES)
    {
        printk("%s: [ERRORE MONTAGGIO MODULO] could not hack %d entries (just %d)\n",MOD_NAME,HACKED_ENTRIES,ret); 
        return -1;      
    }

	unprotect_memory();

    for(i=0;i<HACKED_ENTRIES;i++)
    {
        ((unsigned long *)the_syscall_table)[restore[i]] = (unsigned long)new_sys_call_array[i];
    }

	protect_memory();

    printk("%s: [MONTAGGIO MODULO] all new system-calls correctly installed on sys-call table\n",MOD_NAME);

    ret = register_filesystem(&soafs_fs_type);

    if (likely(ret == 0))
    {
        printk("%s: [MONTAGGIO MODULO] File System 'soafs' registrato correttamente.\n",MOD_NAME);
    }    
    else
    {
        printk("%s: [ERRORE MONTAGGIO MODULO] Errore nella registrazione del File System 'soafs'. - Errore: %d\n", MOD_NAME,ret);
    }

    return ret;
}

static void soafs_exit(void)
{
    int ret;
    int i;
                
    printk("%s: [SMONTAGGIO MODULO] shutting down\n",MOD_NAME);

	unprotect_memory();

    for(i=0;i<HACKED_ENTRIES;i++)
    {
        ((unsigned long *)the_syscall_table)[restore[i]] = the_ni_syscall;
    }

	protect_memory();

    printk("%s: [SMONTAGGIO MODULO] sys-call table restored to its original content\n",MOD_NAME);

    ret = unregister_filesystem(&soafs_fs_type);

    if (likely(ret == 0))
    {
        printk("%s: [SMONTAGGIO MODULO] Rimozione della tipologia di File System 'soafs' avvenuta con successo.\n",MOD_NAME);
    }
    else
    {
        printk("%s: [ERRORE SMONTAGGIO MODULO] Errore nella rimozione della tipologia di File System 'soafs' - Errore: %d\n", MOD_NAME, ret);
    }

}



module_init(soafs_init);
module_exit(soafs_exit);
