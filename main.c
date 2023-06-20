#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>	        /* Needed for KERN_INFO */
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/syscalls.h>
#include <linux/slab.h>             /* kmalloc() */
#include <linux/uaccess.h>          /* copy_from_user() */
#include <linux/buffer_head.h>      /* sb_bread()       */
#include "lib/include/scth.h"
#include <linux/string.h>           /* strncpy() */
#include "./headers/main_header.h"

#define SYNC


MODULE_LICENSE(LICENSE);
MODULE_AUTHOR(AUTHOR);
MODULE_DESCRIPTION(DESCRIPTION);

/*
 * Questo mutex mi consente di gestire un thread alla volta il caricamento delle informazioni
 * relative ai blocchi liberi all'interno della lista.
 */
static DEFINE_MUTEX(free_list_mutex);


unsigned long the_syscall_table = 0x0;
module_param(the_syscall_table, ulong, 0660);
unsigned long the_ni_syscall;
unsigned long new_sys_call_array[] = {0x0, 0x0, 0x0};
int restore[HACKED_ENTRIES] = {[0 ... (HACKED_ENTRIES-1)] -1};



/*
 * check_offset - Controlla il valore dell'offset del blocco di dati
 *
 * @offset: Offset del blocco di dati
 * @sbi: Struttura dati contenente le informazioni FS Specif
 *
 * Restituisce il valore 0 in caso di offset corretto; altrimenti, restituisce 1.
 */
int check_offset(int offset, struct soafs_sb_info *sbi)
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



/*
 * check_size - Controlla il valore della size
 *
 * @size: La dimensione del messaggio
 *
 * Restituisce il valore 0 in caso di size corretta; altrimenti, restituisce 1.
 */
int check_size(size_t size)
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
    size_t byte_ret;
    size_t byte_copy; 
    unsigned short dim;
    struct buffer_head *bh;
    struct soafs_sb_info *sbi;
    struct soafs_block *b;
    
#ifdef LOG
    LOG_SYSTEM_CALL("GET_DATA", "get_data");
#endif

    /* Recupero le informazioni FS specific */
    sbi = (struct soafs_sb_info *)sb_global->s_fs_info;
    
    /* Controllo la validità dei parametri che sono stati passati dall'utente */
    if( check_offset(offset, sbi) || check_size(size) )
    {
        LOG_PARAM_ERR("GET_DATA", "get_data");
        return -EINVAL;
    }

    if(!is_mounted)
    {
        LOG_DEV_ERR("GET_DATA", "get_data");
        return -ENODEV;
    }
    
    /* Comunica la sua presenza ad un eventuale thread che deve eseguire lo smontaggio */
    __sync_fetch_and_add(&(num_threads_run),1);

    /* Verifico se può effettivamente eseguire le proprie attività */
    if(stop)
    {
        wake_up_umount();
        LOG_DEV_ERR("GET_DATA", "get_data");
        return -ENODEV;
    }
   
    /*
     * Verifico se il blocco da leggere che è stato richiesto dall'utente è valido oppure libero.
     * Se è libero, allora la system call termina con un errore.
     */
    if(!check_bit(offset))
    {

#ifdef NOT_CRITICAL_BUT_GET
        printk("%s: [ERRORE GET DATA] E' stata richiesta la lettura del blocco %lld ma il blocco è libero\n", MOD_NAME, offset);
#endif
        wake_up_umount();
        return -ENODATA;
    }

    /* Recupero il messaggio contenuto nel blocco richiesto */

    bh = sb_bread(sb_global, 2 + sbi->num_block_state + offset);

    if(bh == NULL)
    {
        printk("%s: [ERRORE GET DATA] Errore nella lettura del blocco %lld dal dispositivo\n", MOD_NAME, offset);
        wake_up_umount();
        return -EIO;
    }

    b = (struct soafs_block *)bh->b_data;

    dim = b->dim;

    /* Determino quanti bytes devono effettivamente essere copiati per l'utente */
    if(size > dim)
        byte_copy = dim;
    else
        byte_copy = size;      
    
    /* Copio i dati verso l'utente */
    byte_ret = copy_to_user(destination, b->msg, byte_copy);

    wake_up_umount();

#ifdef NOT_CRITICAL_BUT_GET
    printk("%s: [GET DATA] Il messaggio del blocco %lld è stato consegnato con successo\n", MOD_NAME, offset);
#endif

    return (byte_copy - byte_ret);	
}



#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(2, _put_data, char *, source, size_t, size){
#else
asmlinkage uint64_t sys_put_data(char * source, size_t size){
#endif
    int n;
    int ret;  
    int available_data;
    size_t msg_size; 
    uint64_t index;
    struct block_free *item;
    struct soafs_sb_info *sbi;

#ifdef LOG
    LOG_SYSTEM_CALL("PUT_DATA", "put_data");
#endif

    /* Eseguo il controllo sulla dimensione richiesta dall'utente */
    if(check_size(size))
    {
        LOG_PARAM_ERR("PUT_DATA", "put_data");
        return -EINVAL;
    }

    if(!is_mounted)
    {
        LOG_DEV_ERR("GET_DATA", "get_data");
        return -ENODEV;
    }
    
    /* Comunica la sua presenza ad un eventuale thread che deve eseguire lo smontaggio */
    __sync_fetch_and_add(&(num_threads_run),1);

    /* Verifico se può effettivamente eseguire le proprie attività */
    if(stop)
    {
        wake_up_umount();
        LOG_DEV_ERR("GET_DATA", "get_data");
        return -ENODEV;
    }

    /* Recupero le informazioni FS specific */
    sbi = (struct soafs_sb_info *)sb_global->s_fs_info;

    /* Verifico se i blocchi sono già tutti occupati */

    if( (head_free_block_list == NULL) && (num_block_free_used == sbi->num_block_free) )
    {

#ifdef NOT_CRITICAL_BUT_PUT
        printk("%s: [ERRORE PUT DATA] Non ci sono più blocchi liberi da utilizzare\n", MOD_NAME);
#endif
        wake_up_umount();
        return -ENOMEM;
    }

    item = NULL;

    /*
     * Recupero l'indice del blocco libero da utilizzare per inserire il messaggio dell'utente.
     * Se la lista dei blocchi liberi è vuota, allora devo recuperare le informazioni relative
     * ad altri blocchi liberi; altrimenti, estraggo il blocco in testa alla lista.
     */
    if(head_free_block_list == NULL)
    {
        n = 0;
retry:
        if(n > 5)
        {
            printk("%s: [ERRORE PUT DATA] Numero di tentativi esaurito per il recupero di un blocco libero.\n", MOD_NAME);
            wake_up_umount();
            return -ENOMEM;
        }

        /*
         * Devo trovare nuovi blocchi liberi tra quelli che erano liberi al tempo di montaggio e
         * che finora non ho mai utilizzato (i.e., il cui indice non è stato mai caricato nella lista).
         */

        mutex_lock(&free_list_mutex);

        /* Recupero gli indici dei blocchi liberi */

        ret = get_bitmask_block();

        mutex_unlock(&free_list_mutex);

        if(ret)
        {
                printk("%s: [ERRORE PUT DATA] Errore esecuzione kmalloc() nel recupero di un blocco libero\n", MOD_NAME);
                wake_up_umount();
                return -EIO;    
        }
  
        /*
         * Per via della concorrenza, è possibile che la testa della lista risulti essere NULL.
         * Se sono stati caricati nella lista dei blocchi liberi tutti gli indici liberi all'istante
         * di montaggio, allora non esistono blocchi liberi da utilizzare.
         */
        if( (head_free_block_list == NULL) && (num_block_free_used == sbi->num_block_free) )
        {

#ifdef NOT_CRITICAL_BUT_PUT
                printk("%s: [ERRORE PUT DATA] Non ci sono più blocchi liberi da utilizzare\n", MOD_NAME);
#endif
                wake_up_umount();
                return -ENOMEM;
        }

        /* La lista si è nuovamente svuotata ma provo a caricarci nuovamente i blocchi liberi rimanenti. */
        if( (head_free_block_list == NULL) && (num_block_free_used < sbi->num_block_free))
        {

#ifdef NOT_CRITICAL_PUT
            printk("%s: [ERRORE PUT DATA] Tentativo #%d fallito per il recupero dei blocchi liberi\n", MOD_NAME, n);
#endif
            n++;
            goto retry;
        }
    }

    /* Prendo l'indice in testa alla lista dei blocchi liberi */
    item = get_freelist_head();
    
    if(item == NULL)
    {
        printk("%s: [ERRORE PUT DATA] Errore nel recupero di un blocco libero\n", MOD_NAME);

        wake_up_umount();
        return -ENOMEM;
    }

    /* Recupero l'indice del blocco libero da utilizzare per scrivere il nuovo messaggio */
    index = item -> block_index;

    /* Calcolo la dimensione massima del messaggio che può essere memorizzato nel blocco */

    available_data = SOAFS_BLOCK_SIZE - (sizeof(uint64_t) + sizeof(unsigned short));

    /* Determino la quantità di byte utente che dovranno essere scritti nel blocco */

    if(size > available_data)
        msg_size = available_data;  
    else
        msg_size = size;

    ret = insert_new_data_block(index, source, msg_size);

    if(ret)
    {
        printk("%s: [ERRORE PUT DATA] Errore nell'inserimento del blocc %lld all'interno della Sorted List\n", MOD_NAME, index);
        insert_free_list_conc(item);
        wake_up_umount();
        return -EIO;
    }
   
    /* Dealloco l'elemento rimosso precedentemente dalla Free List */
    kfree(item);
    
    set_bitmask(index, 1);    

#ifdef NOT_CRITICAL_BUT_PUT
    printk("%s: [PUT DATA] Il messaggio '%s' è stato inserito con successo nel blocco %lld\n", MOD_NAME, msg, index);
#endif

    wake_up_umount();

    return index;	
}



#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(1, _invalidate_data, uint64_t, offset){
#else
asmlinkage int sys_invalidate_data(uint64_t offset){
#endif

    int ret;
    struct soafs_sb_info *sbi;

#ifdef LOG
    LOG_SYSTEM_CALL("INVALIDATE DATA", "invalidate_data");
#endif

    sbi = (struct soafs_sb_info *)sb_global->s_fs_info;

    /* Eseguo il controllo sull'offset del blocco richiesto dall'utente */
    if( check_offset(offset, sbi) )
    {
        LOG_PARAM_ERR("INVALIDATE DATA", "invalidate_data");
        return -EINVAL;
    }

    if(!is_mounted)
    {
        LOG_DEV_ERR("GET_DATA", "get_data");
        return -ENODEV;
    }
    
    /* Comunica la sua presenza ad un eventuale thread che deve eseguire lo smontaggio */
    __sync_fetch_and_add(&(num_threads_run),1);

    /* Verifico se può effettivamente eseguire le proprie attività */
    if(stop)
    {
        wake_up_umount();
        LOG_DEV_ERR("GET_DATA", "get_data");
        return -ENODEV;
    }

    /*
     * Verifico la validità del blocco richiesto dall'utente. Se il blocco è
     * già non valido allora la system call termina immediatamente con un codice
     * di errore.
     */
    if(!check_bit(offset))
    {

#ifdef NOT_CRITICAL_INVAL
        printk("%s: [ERRORE INVALIDATE DATA] E' stata richiesta l'invalidazione del blocco %lld ma il blocco non è valido\n", MOD_NAME, offset);
#endif
        wake_up_umount();
        return -ENODATA;
    }

    /* Rimuovi il blocco dalla lista Sorted List e modifica il bit di stato nella bitmask */
    ret = invalidate_data_block(offset);

    if(ret)
    {

#ifdef NOT_CRITICAL_BUT_INVAL
        printk("%s: [ERRORE INVALIDATE DATA] L'invalidazione del blocco %lld non è stata eseguita con successo\n", MOD_NAME, offset);  
#endif       
        wake_up_umount();

        return -ENODATA;
    }


#ifdef NOT_CRITICAL_BUT_INVAL
    printk("%s: [INVALIDATE DATA] L'invalidazione del blocco %lld è stata eseguita con successo\n", MOD_NAME, offset);
#endif

    wake_up_umount();

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
        printk("%s: [MONTAGGIO MODULO] Indirizzo della system call table ricevuto %px\n",MOD_NAME,(void*)the_syscall_table);
     	printk("%s: [MONTAGGIO MODULO] Inizializzazione - entry da hackerare %d\n",MOD_NAME,HACKED_ENTRIES);
	}

	new_sys_call_array[0] = (unsigned long)sys_get_data;
    new_sys_call_array[1] = (unsigned long)sys_put_data;
    new_sys_call_array[2] = (unsigned long)sys_invalidate_data;

    ret = get_entries(restore,HACKED_ENTRIES,(unsigned long*)the_syscall_table,&the_ni_syscall);

    if (ret != HACKED_ENTRIES)
    {
        printk("%s: [ERRORE MONTAGGIO MODULO] Non posso hackerare %d entry (solamente %d)\n",MOD_NAME,HACKED_ENTRIES,ret); 
        return -1;      
    }

	unprotect_memory();

    for(i=0;i<HACKED_ENTRIES;i++)
    {
        ((unsigned long *)the_syscall_table)[restore[i]] = (unsigned long)new_sys_call_array[i];
    }

	protect_memory();

    printk("%s: [MONTAGGIO MODULO] Tutte le nuove system calls sono state correttamente installate sulla sys-call table\n",MOD_NAME);

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

    printk("%s: [SMONTAGGIO MODULO] La sys-call table è stata ripristinata al suo contenuto originale\n",MOD_NAME);

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
