#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>	/* Needed for KERN_INFO */
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/syscalls.h>
#include <linux/slab.h>         /* kmalloc() */
#include <linux/uaccess.h>      /* copy_from_user() */
#include <linux/buffer_head.h>  /* sb_bread()       */

#include <linux/log2.h>         /* ilog2()  */

#include "lib/include/scth.h"
#include "./headers/main_header.h"



MODULE_LICENSE(LICENSE);
MODULE_AUTHOR(AUTHOR);
MODULE_DESCRIPTION(DESCRIPTION);



unsigned long the_syscall_table = 0x0;
module_param(the_syscall_table, ulong, 0660);
unsigned long the_ni_syscall;
unsigned long new_sys_call_array[] = {0x0, 0x0, 0x0};
int restore[HACKED_ENTRIES] = {[0 ... (HACKED_ENTRIES-1)] -1};



static int check_offset(int offset)
{
    /*
     * Tengo conto dei primi due blocchi, contenenti rispettivamente
     * il superblocco e l'inode del file, e del fatto che il primo
     * blocco ha indice pari a 0 (e non 1) e quindi l'offset può assumere
     * valore 0.
     */
    if( ((offset + 2) > (NBLOCKS - 1)) || (offset < 0) )
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



/*
 * Computa il numero di byte all'interno di un
 * blocco del device che sono riservati per
 * mantenere il messaggio utente. Gli eventuali
 * altri bytes vengono utilizzati per mantenere i
 * metadati.
 */
static int get_available_data(void)
{
    return SOAFS_BLOCK_SIZE - METADATA;
}



static int get_metadata(void)
{
    return METADATA;
}



#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(3, _get_data, int, offset, char *, destination, size_t, size){
#else
asmlinkage int sys_get_data(int offset, char * destination, size_t size){
#endif

    struct buffer_head *bh = NULL;
    int ret;
    int real_offset;
    int str_len;
    int available_data;
    size_t byte_ret;
    size_t byte_copy;                                           /* Il numero di byte che verranno restituiti all'utente */
    char *msg_block;                                            /* Il messaggio contenuto all'interno del blocco del device */   
    
    LOG_SYSTEM_CALL("get_data");
    
    ret = check_is_mounted();                                   /* verifico se il file system su cui si deve operare è stato effettivamente montato */

    if(!ret)
    {
        LOG_DEV_ERR("get_data");
        return -ENODEV;
    }

    if( check_offset(offset) || check_size(size) )
    {
        LOG_PARAM_ERR("get_data");
        return -EINVAL;
    }

    real_offset = offset + 2;                                   /* Non considero i blocchi 1 e 2 che contengono rispettivamente il superblocco e l'inode */

    bh = sb_bread(sb_global, real_offset);          /* Utilizziamo il buffer cache per caricare il blocco 'offset' richiesto */                      

    if(bh == NULL)
    {
        LOG_BH("get_data", "lettura", real_offset, "errore");
        return -EIO;
    }

    LOG_BH("get_data", "lettura", real_offset, "successo");

    msg_block = (char *)bh->b_data;                 /* Prendo il riferimento al messaggio contenuto nel blocco portato in memoria */

    //TODO: gestisce gli eventuali metadati.

    msg_block = msg_block + get_metadata();

    str_len = strlen(msg_block);                    /* Non tiene conto del terminatore di stringa */

    printk("%s: il messaggio letto dal blocco del device è '%s' ed ha una dimensione di %d\n", MOD_NAME, msg_block, str_len);

    available_data = get_available_data();          /* La dimensione massima del messsaggio che può essere mantenuto nei blocchi del device */

    if(size > available_data)
    {
        /*
        * Se la quantità di dati che mi stai chiedendo
        * è strettamente maggiore della dimensione massima
        * di un messaggio che può essere memorizzato, allora
        * ti restituisco l'intero contenuto del blocco. Il
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
         * più lungo della dimensione 'size' richiesta. Sono
         * le mie system call che si fanno carico della gestione
         * corretta del terminatore di stringa.
         */
        byte_copy = size;

        /*
         * Se possibile, cerco di evitare l'operazione di scrittura.
         * Per come vengono scritti i messaggi all'interno del blocco,
         * il terminatore di stringa è già presente.
         */
        if(size < available_data)
        {
            msg_block[byte_copy-1] = '\0';
        }
      
    }    

    printk("Numero di bytes che verranno effettivamente restituiti è %ld\n", byte_copy);
    
    byte_ret = copy_to_user(destination, msg_block, byte_copy);

    brelse(bh);                                     /* Rilascio del buffer cache */
    
    return (byte_copy - byte_ret);
	
}



#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(2, _put_data, char *, source, size_t, size){
#else
asmlinkage int sys_put_data(char * source, size_t size){
#endif

    struct buffer_head *bh = NULL;
    int available_data;
    char *msg_block;
    int ret;    
    char *msg;                      /* Il messaggio che si richiede di scrivere nel blocco */
    size_t msg_size;                /* La dimensione del messaggio che verrà scritto nel blocco */    
    size_t bytes_to_copy;           /* Il numero di bytes che verranno effettivamente copiati dallo spazio utente */    
    unsigned long bytes_ret;        /* Il numero di bytes effettivamente copiati */

    LOG_SYSTEM_CALL("put_data");

    ret = check_is_mounted();   /* verifico se il file system su cui si deve operare è stato effettivamente montato */

    if(!ret)
    {
        LOG_DEV_ERR("get_data");
        return -ENODEV;
    }

    if( check_size(size) )
    {
        LOG_PARAM_ERR("get_data");
        return -EINVAL;
    }

    available_data = get_available_data();        /* La dimensione massima del messsaggio che può essere mantenuto nei blocchi del device */

    if(size > available_data)
    {
        /*
         * Si sta chiedendo di scrivere un messaggio
         * che ha la dimensione maggiore della dimensione
         * massima per un messaggio che viene scritto nel
         * device. Di conseguenza, il messaggio verrà scritto
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


    msg = (char *)kmalloc(msg_size, GFP_KERNEL);        /* Alloco la memoria per ospitare il messaggio proveniente dallo spazio utente */

    if(msg == NULL)
    {
        LOG_KMALLOC_ERR("put_data");
        return -EIO;    
    }

    bytes_ret = copy_from_user(msg, source, bytes_to_copy);

    printk("Sono stati copiati %ld bytes\n", bytes_ret);

    if(bytes_to_copy!=msg_size)
    {
        msg[msg_size - 1] = '\0';                           /* Inserisco il terminatore di stringa */
    }

    printk("%s: E' stato richiesto di scrivere il messaggio '%s'\n", MOD_NAME, msg);

    printk("%s: Il file system su cui si sta lavorando è di tipo %s\n", MOD_NAME, sb_global->s_type->name);

    //TODO: determina, se esiste, un blocco libero per la scrittura del messaggio
    bh = sb_bread(sb_global, 2);                            

    if(bh == NULL)
    {
        LOG_BH("put_data", "scrittura", 2, "errore");
        return -EIO;
    }

    LOG_BH("put_data", "scrittura", 2, "successo");  

    msg_block = (char *)bh->b_data;                 /* Prendo il messaggio contenuto nel blocco */

    memcpy(msg_block, msg, msg_size);

    mark_buffer_dirty(bh);                          /* Marco il buffer cache come DIRTY per flushare i dati */

    brelse(bh);                                     /* Rilascio del buffer cache */

    return bytes_to_copy - bytes_ret;
	
}



//TODO: Implementa la system call
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(1, _invalidate_data, int, offset){
#else
asmlinkage int sys_invalidate_data(int offset){
#endif

    int ret;
    int log;

    LOG_SYSTEM_CALL("invalidate_data");

    ret = check_is_mounted();

    if(!ret)
    {
        LOG_DEV_ERR("get_data");
        return -ENODEV;
    }

    log = ilog2(offset);

    printk("%s: Il valore del logaritmo pari a %d.\n", MOD_NAME, log);

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
        printk("%s: received sys_call_table address %px\n",MOD_NAME,(void*)the_syscall_table);
     	printk("%s: initializing - hacked entries %d\n",MOD_NAME,HACKED_ENTRIES);
	}

	new_sys_call_array[0] = (unsigned long)sys_get_data;
    new_sys_call_array[1] = (unsigned long)sys_put_data;
    new_sys_call_array[2] = (unsigned long)sys_invalidate_data;

    ret = get_entries(restore,HACKED_ENTRIES,(unsigned long*)the_syscall_table,&the_ni_syscall);

    if (ret != HACKED_ENTRIES)
    {
        printk("%s: could not hack %d entries (just %d)\n",MOD_NAME,HACKED_ENTRIES,ret); 
        return -1;      
    }

	unprotect_memory();

    for(i=0;i<HACKED_ENTRIES;i++)
    {
        ((unsigned long *)the_syscall_table)[restore[i]] = (unsigned long)new_sys_call_array[i];
    }

	protect_memory();

    printk("%s: all new system-calls correctly installed on sys-call table\n",MOD_NAME);

    ret = register_filesystem(&soafs_fs_type);

    if (likely(ret == 0))
    {
        printk("%s: File System 'soafs' registrato correttamente.\n",MOD_NAME);
    }    
    else
    {
        printk("%s: Errore nella registrazione del File System 'soafs'. - Errore: %d\n", MOD_NAME,ret);
    }

    return ret;
}

static void soafs_exit(void)
{
    int ret;
    int i;
                
    printk("%s: shutting down\n",MOD_NAME);

	unprotect_memory();

    for(i=0;i<HACKED_ENTRIES;i++)
    {
        ((unsigned long *)the_syscall_table)[restore[i]] = the_ni_syscall;
    }

	protect_memory();

    printk("%s: sys-call table restored to its original content\n",MOD_NAME);

    ret = unregister_filesystem(&soafs_fs_type);

    if (likely(ret == 0))
    {
        printk("%s: Rimozione della tipologia di File System 'soafs' avvenuta con successo.\n",MOD_NAME);
    }
    else
    {
        printk("%s: Errore nella rimozione della tipologia di File System 'soafs' - Errore: %d\n", MOD_NAME, ret);
    }

}

module_init(soafs_init);
module_exit(soafs_exit);
