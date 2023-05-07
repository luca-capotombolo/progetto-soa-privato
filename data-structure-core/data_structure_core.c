#include <linux/buffer_head.h>  /* sb_bread()-brelse() */
#include <linux/string.h>       /* strncpy() */
#include <linux/slab.h>         /* kmalloc() */
#include <linux/wait.h>         /* wait_event_interruptible() - wake_up_interruptible() */

#include "../headers/main_header.h"



struct block *head_sorted_list = NULL;              /* Puntatore alla testa della lista contenente i blocchi nell'ordine di consegna */
struct block_free *head_free_block_list = NULL;     /* Puntatore alla testa della lista contenente i blocchi liberi */
struct ht_valid_entry *hash_table_valid = NULL;     /* Hash table */
uint64_t num_block_free_used = 0;                   /* Numero di blocchi liberi al tempo di montaggio caricati nella lista */
uint64_t pos = 0;                                   /* Indice da cui iniziare la ricerca dei nuovi blocchi liberi */
uint64_t**bitmask = NULL;                           /* Bitmask */
int x = 0;                                          /* Numero righe della Tabella Hash */
struct grace_period *gp = NULL;                     /* Strtuttura dati per il Grace Period */
uint64_t sync_var = 0;                              /* Variabile per la sincronizzazione inserimenti-invalidazioni */




void scan_free_list(void) //
{
    struct block_free *curr;

    curr = head_free_block_list;

    printk("%s: ------------------------------ INIZIO FREE LIST ------------------------------------------", MOD_NAME);

    while(curr!=NULL)
    {
        printk("Blocco #%lld\n", curr->block_index);
        curr = curr->next;
    }

    printk("%s: ---------------------------- FINE FREE LIST ----------------------------------------------", MOD_NAME);
}



void scan_sorted_list(void) //
{
    struct block *curr;

    curr = head_sorted_list;
    
    printk("%s: ---------------------------------- INIZIO SORTED LIST ---------------------------------------------", MOD_NAME);

    while(curr!=NULL)
    {
        printk("Blocco #%lld - Messaggio %s\n", curr->block_index, curr->msg);
        curr = curr->sorted_list_next;
    }

    printk("%s: ---------------------------------- FINE SORTED LIST   ---------------------------------------------", MOD_NAME);
    
}



void scan_hash_table(void) //
{
    int entry_num;
    struct ht_valid_entry entry;
    struct block *item;
    
    printk("%s:-------------------------- INIZIO HASH TABLE --------------------------------------------------\n", MOD_NAME);

    for(entry_num=0; entry_num<x; entry_num++)
    {
        printk("%s: ----------------------------- %d --------------------------------", MOD_NAME, entry_num);
        entry = hash_table_valid[entry_num];
        item = entry.head_list;

        while(item!=NULL)
        {
            printk("%s: Blocco #%lld\n", MOD_NAME, item->block_index);
            item = item ->hash_table_next;
        }

        printk("%s: -------------------------------------------------------------------", MOD_NAME);
        
    }

    printk("%s: -------------------------- FINE HASH TABLE ---------------------------------------------------\n", MOD_NAME);
}



void debugging_init(void) //
{
    /* scansione della lista contenente le informazioni dei blocchi liberi. */
    scan_free_list();

    /* scansione della lista contenente i blocchi ordinati per inserimento. */
    scan_sorted_list();

    /* scansione delle liste della hash table */
    scan_hash_table();
}



/*
 * Computa il numero di righe della tabella hash
 * applicando la formula. Si cerca di avere un numero
 * logaritmico di elementi per ogni lista della
 * hast_table_valid.
 */
void compute_num_rows(uint64_t num_data_block)
{
    int list_len;
    
    if(num_data_block == 1)
    {
        x = 1;      /* Non devo applicare la formula e ho solamente una lista */
    }
    else
    {
 
        list_len = ilog2(num_data_block) + 1;    /* Computo il numero di elementi massimo che posso avere in una lista */

        if((num_data_block % list_len) == 0)
        {
            x = num_data_block / list_len;
        }
        else
        {
            x = (num_data_block / list_len) + 1;
        }
    }

    printk("%s: [COMPUTAZIONE NUMERO RIGHE] La lunghezza massima di una entry della tabella hash è pari a %d.\n", MOD_NAME, list_len);
    printk("%s: [COMPUTAZIONE NUMERO RIGHE] Il numero di entry nella tabella hash è pari a %d.\n", MOD_NAME, x);
}



/*
 * Questa funzione ha il compito di recuperare il messaggio
 * all'interno del blocco con indice pari a offset.
 */
char * read_data_block(uint64_t offset, struct ht_valid_entry *entry)
{

    struct block *item;
    char *str;
    size_t len;

    item = entry->head_list;

    while(item!=NULL)
    {
        if(item->block_index == offset)
            break;
        
        item = item ->hash_table_next;
    }

    if(item == NULL)
    {

#ifdef NOT_CRITICAL_BUT_GET
        printk("%s: [ERRORE GET DATA] Il blocco richiesto %lld è stato invalidato in concorrenza\n", MOD_NAME, offset);
#endif

        return NULL;
    }
    
    len = strlen(item->msg)+1;

    str = (char *)kmalloc(len, GFP_KERNEL);

    if(str==NULL)
    {
        printk("%s: [ERRORE GET DATA] Errore esecuzoine malloc() per la copia della stringa blocco %lld\n", MOD_NAME, offset);
        return NULL;
    }

    strncpy(str, item->msg, len);

    return str;
}



/* E' la versione concorrente della funzione 'insert_free_list' */
void insert_free_list_conc(struct block_free *item)
{
    struct block_free *old_head;
    int ret;

retry_insert_free_list_conc:

    old_head = head_free_block_list;

    item->next = old_head;

    ret = __sync_bool_compare_and_swap(&(head_free_block_list), old_head, item);

    if(!ret)
    {
        goto retry_insert_free_list_conc;
    }

#ifdef NOT_CRITICAL_BUT
    printk("%s: [INSERT FREE LIST] L'inserimento in testa del blocco %lld avvenuto con successo\n", MOD_NAME, item->block_index);
#endif
}



/*
 * Eliminazione del blocco con indice passato come parametro
 * sia dalla lista nella HT che dalla lista ordinata.
 */
struct result_inval * remove_block(uint64_t index)
{
    int num_entry_ht;
    struct ht_valid_entry *ht_entry;
    struct block *next;
    struct block *curr;
    struct block *prev;
    struct result_inval *res_inval;

    res_inval = (struct result_inval *)kmalloc(sizeof(struct result_inval), GFP_KERNEL);

    if(res_inval == NULL)
    {
        printk("%s: [ERRORE INVALIDATE DATA - REMOVE] Errore esecuzione kmalloc() nella invalidazione del blocco %lld\n", MOD_NAME, index);

        return NULL;
    }
    

    /* Identifico la lista corretta nella hash table per effettuare l'inserimento */
    num_entry_ht = index % x;

    ht_entry = &(hash_table_valid[num_entry_ht]);

    curr = ht_entry->head_list;

    /* Gestione di invalidazioni */
    if(curr == NULL)
    {
#ifdef NOT_CRITICAL_BUT
        printk("%s: [ERRORE INVALIDATE DATA - REMOVE] La lista #%d nella HT risulta essere vuota\n", MOD_NAME, num_entry_ht);
#endif

        res_inval->code = 2;

        res_inval->block = NULL;

        return res_inval;
    }

    if(curr->block_index == index)
    {
        ht_entry->head_list = curr->hash_table_next;

        asm volatile ("mfence");

#ifdef NOT_CRITICAL
        printk("%s: [INVALIDATE DATA - REMOVE] Il blocco %lld richiesto per l'invalidazione è stato eliminato con successo dalla lista nella HT\n", MOD_NAME, index);
#endif

        goto remove_sorted_list;
    }

    prev = curr;

    curr = prev->hash_table_next;

    /*
     * Ricerco il blocco richiesto da invalidare nella
     * corretta lista della HT.
     */
    while(curr != NULL)
    {
        if(curr->block_index == index)
        {
#ifdef NOT_CRITICAL
            printk("%s: [INVALIDATE DATA - REMOVE] Il blocco %lld da invalidare è stato trovato con successo nella lista della HT\n", MOD_NAME, index);
#endif

            next = curr->hash_table_next;

            break;
        }
        prev = curr;

        curr = curr->hash_table_next;
    }

    if( curr == NULL )
    {
#ifdef NOT_CRITICAL_BUT
        printk("%s: [ERRORE INVALIDATE DATA - REMOVE] Il blocco %lld richiesto per l'invalidazione non è presente nella lista #%d della HT\n", MOD_NAME, index, num_entry_ht);
#endif
        res_inval->code = 2;
        res_inval->block = NULL;
        return res_inval;
    }

    prev->hash_table_next = next;

    asm volatile ("mfence");

#ifdef NOT_CRITICAL
    printk("%s: [INVALIDAZIONE] Il blocco %lld richiesto per l'invalidazione è stato eliminato con successo dalla lista nella HT\n", MOD_NAME, index);
#endif

remove_sorted_list:

    /* 
     * Verifico se il primo blocco nella lista ordinata
     * è quello richiesto da eliminare. La lista non può
     * essere vuota poiché c'è almeno l'elemento da invalidare.
     */

    if(head_sorted_list == NULL)
    {
        printk("%s: [ERRORE INVALIDATE DATA - REMOVE] Errore inconsistenza: il blocco %lld era presente all'interno della lista nella HT ma non nella lista ordinata\n", MOD_NAME, index);

        res_inval->code = 1;

        res_inval->block = NULL;

        return res_inval;
    }

    if( head_sorted_list->block_index == index )
    {
        res_inval->code = 0;

        res_inval->block = head_sorted_list;

        head_sorted_list = head_sorted_list->sorted_list_next;

        asm volatile ("mfence");

#ifdef NOT_CRITICAL
        printk("%s: [INVALIDATE DATA - REMOVE] Il blocco %lld richiesto per l'invalidazione è stato eliminato con successo dalla lista ordinata\n", MOD_NAME, index);
#endif

        return res_inval;
    }

    /* Ricerco il blocco richiesto da invalidare nella lista ordinata */

    prev = head_sorted_list;

    curr = prev->sorted_list_next;

    while(curr!=NULL)
    {
        if(curr->block_index == index)
        {
#ifdef NOT_CRITICAL
            printk("%s: [INVALIDATE DATA - REMOVE] Il blocco %lld da invalidare è stato trovato con successo nella lista ordinata\n", MOD_NAME, index);
#endif

            next = curr->sorted_list_next;

            break;
        }

        prev = curr;

        curr = curr->sorted_list_next;
    }

    if(curr == NULL)
    {
        printk("%s: [ERRORE INVALIDATE DATA - REMOVE] Errore inconsistenza: il blocco %lld era presente all'interno della lista nella HT ma non nella lista ordinata\n", MOD_NAME, index);

        res_inval->code = 1;

        res_inval->block = NULL;

        return res_inval;
    }

    res_inval->code = 0;

    res_inval->block = curr;

    prev->sorted_list_next = next;
 
    asm volatile ("mfence");  

#ifdef NOT_CRITICAL
    printk("%s: [INVALIDATE DATA - REMOVE] Il blocco %lld richiesto per l'invalidazione è stato eliminato con successo dalla lista ordinata\n", MOD_NAME, index);
#endif
    
    return res_inval;
}



/*
 * Questa funzione esegue l'invalidazione del blocco il cui
 * indice è passato come parametro.
 */
int invalidate_block(uint64_t index)
{

    int n ;
    int index_ht;
    int index_sorted;
    uint64_t num_insert;
    uint64_t free_index_block;
    unsigned long updated_epoch_ht;
    unsigned long updated_epoch_sorted;
    unsigned long last_epoch_ht;
    unsigned long last_epoch_sorted;
    unsigned long grace_period_threads_ht;
    unsigned long grace_period_threads_sorted;
    struct result_inval *res_inval;
    struct block_free *bf;


    n = 0;

    /*
     * Prima di effettuare l'invalidazione, è necessario verificare
     * se sono in esecuzione degli inserimenti. Nel caso in cui
     * ci siano degli inserimenti in corso (almeno uno), l'invalidazione
     * dovrà essere posticipata; altrimenti, si comunica l'inzio della
     * invalidazione e si procede con la rimozione del blocco. 
     */

    /* Prendo il mutex per eseguire il processo di invalidazione */
    mutex_lock(&invalidate_mutex);

retry_invalidate:

    if(n > 20)
    {
        printk("%s: [ERRORE INVALIDATE DATA] Il numero massimo di tentativi per l'invalidazione del blocco %lld è stato raggiunto\n", MOD_NAME, index);

        mutex_unlock(&invalidate_mutex);

        return 1;
    }

    /* Sezione critica tra inserimenti e invalidazioni */
    mutex_lock(&inval_insert_mutex);

    /* Recupero il numero di inserimenti in corso */
    num_insert = sync_var & MASK_NUMINSERT;

#ifdef NOT_CRITICAL
    printk("%s: [INVALIDATE DATA] Il numero di inserimenti attualmente in corso è pari a %lld\n", MOD_NAME, num_insert);
#endif

    if(num_insert > 0)
    {
        mutex_unlock(&inval_insert_mutex);

#ifdef NOT_CRITICAL_BUT
        printk("%s: [ERRORE INVALIDATE DATA] L'invalidazione del blocco %lld non è stata effettuata al tentativo #%d\n", MOD_NAME, index, n);

        printk("%s: [ERRORE INVALIDATE DATA] Il thread per l'invalidazione del blocco %lld viene messo in attesa\n", MOD_NAME, index);
#endif

        wait_event_interruptible(the_queue, (sync_var & MASK_NUMINSERT) == 0);

        n++;

#ifdef NOT_CRITICAL
        printk("%s: [ERRORE INVALIDATE DATA] Nuovo tentativo #%d di invalidazione del blocco %lld\n", MOD_NAME, n, index);
#endif

        goto retry_invalidate;
    }

    if(sync_var)
    {
        printk("%s: [ERRORE INVALIDATE DATA] Problema di incosistenza: invalidazione e inserimento paralleli\n", MOD_NAME);

        mutex_unlock(&inval_insert_mutex);

        mutex_unlock(&invalidate_mutex);

        return 1;
    }

    /* Comunico l'inizio del processo di invalidazione */
    __atomic_exchange_n (&(sync_var), 0X8000000000000000, __ATOMIC_SEQ_CST);

    mutex_unlock(&inval_insert_mutex);

    /*
     * Durante il processo di invalidazione non è possibile
     * avere in concorrenza l'inserimento di un nuovo blocco.
     */
    res_inval = remove_block(index);

    if( res_inval == NULL )
    {
        __atomic_exchange_n (&(sync_var), 0X0000000000000000, __ATOMIC_SEQ_CST);

        mutex_unlock(&invalidate_mutex);

        wake_up_interruptible(&the_queue);

        return 1;
    }

    if(res_inval->code == 2)
    {
        __atomic_exchange_n (&(sync_var), 0X0000000000000000, __ATOMIC_SEQ_CST);

        mutex_unlock(&invalidate_mutex);

        wake_up_interruptible(&the_queue);

        return 0;
    }

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

#ifdef NOT_CRITICAL
    printk("%s: [INVALIDATE DATA] Attesa della terminazione del grace period HT: #threads %ld\n", MOD_NAME, grace_period_threads_ht);
    printk("%s: [INVALIDATE DATA] Attesa della terminazione del grace period lista ordinata: #threads %ld\n", MOD_NAME, grace_period_threads_sorted);
#endif

sleep_again:

    wait_event_interruptible(the_queue, (gp->standing_ht[index_ht] >= grace_period_threads_ht) && (gp->standing_sorted[index_sorted] >= grace_period_threads_sorted));

#ifdef NOT_CRITICAL
    printk("%s: gp->standing_ht[index_ht] = %ld\tgrace_period_threads_ht = %ld\tgp->standing_sorted[index_sorted] = %ld\tgrace_period_threads_sorted = %ld\n", MOD_NAME, gp->standing_ht[index_ht], grace_period_threads_ht, gp->standing_sorted[index_sorted], grace_period_threads_sorted);
#endif

    if((gp->standing_ht[index_ht] < grace_period_threads_ht) || (gp->standing_sorted[index_sorted] < grace_period_threads_sorted))
    {
        printk("%s: [ERRORE INVALIDATE DATA] Il thread invalidate va nuovamente a dormire per l'invalidazione del blocco %lld\n", MOD_NAME, index);
        goto sleep_again;
    }

    gp->standing_sorted[index_sorted] = 0;

    gp->standing_ht[index_ht] = 0;

    if(res_inval->block != NULL)
    {
        free_index_block = res_inval->block->block_index;

        kfree(res_inval->block);
#ifdef NOT_CRITICAL
        printk("%s: [INVALIDATE DATA] Deallocazione del blocco %lld eliminato con successo\n", MOD_NAME, index);
#endif
    }

retry_kmalloc_invalidate_block:

    /* Inserisco l'indice del blocco nella lista dei blocchi liberi */
    bf = (struct block_free *)kmalloc(sizeof(struct block_free), GFP_KERNEL);

    if(bf == NULL)
    {
        printk("%s: [ERRORE INVALIDATE DATA] Errore esecuzione kmalloc() a seguito della rimozione\n", MOD_NAME);

        goto retry_kmalloc_invalidate_block;
    }

    
    bf->block_index = free_index_block;

    bf->next = NULL;

    insert_free_list_conc(bf);

    set_bitmask(index,0);

    __atomic_exchange_n (&(sync_var), 0X0000000000000000, __ATOMIC_SEQ_CST);

    /* Rilascio il mutex per permettere successive invalidazioni */
    mutex_unlock(&invalidate_mutex);

    wake_up_interruptible(&the_queue);

    return 0;
}



/*
 * Questa funzione ha il compito di inizializzare la struttura
 * dati della bitmask per mantenere le informazioni sulla validità
 * dei blocchi del dispositivo.
 */
int init_bitmask(void)
{
    struct buffer_head *bh;
    struct soafs_sb_info *sbi;
    uint64_t num_block_state;
    uint64_t index;
    uint64_t roll_index;
    uint64_t * block_state;
    int sub_index;

    /* 
     * Recupero le informazioni specifiche 
     * necessarie per identificare dove nel
     * device sono presenti i blocchi di stato
     */

    sbi = (struct soafs_sb_info *)sb_global->s_fs_info;

    num_block_state = sbi->num_block_state;

    if(num_block_state <= 0)
    {
        printk("%s: [ERRORE INIZIALIZZAZIONE CORE - BITMASK] Numero dei blocchi di stato non è valido.\n", MOD_NAME);

        return 1;
    }

    printk("%s: [INIZIALIZZAZIONE CORE - BITMASK] Inizio inizializzazione bitmask...Il numero delle entry è pari a %lld\n", MOD_NAME, num_block_state);

    bitmask = (uint64_t **)kzalloc(num_block_state * sizeof(uint64_t *), GFP_KERNEL);

    if(bitmask == NULL)
    {
        printk("%s: [ERRORE INIZIALIZZAZIONE CORE - BITMASK] Errore esecuzione kzalloc() durante l'allocazione della bitmask.", MOD_NAME);

        return 1;
    }


    /* [SB][Inode][SB1]...[SBn][DB]...[DB] */
    for(index=0;index<num_block_state;index++)
    {
        /* Recupero i bit di stato dal device */
        bh = sb_bread(sb_global, index + 2);

        if(bh == NULL)
        {
            printk("%s: [ERRORE INIZIALIZZAZIONE CORE - BITMASK] Errore nella lettura del blocco di stato con indice %lld.\n", MOD_NAME, index);

            for(roll_index=0; roll_index<index; roll_index++)
            {
                kfree(bitmask[roll_index]);    
            }

            kfree(bitmask);

            bitmask = NULL;

            printk("%s: [ERRORE INIZIALIZZAZIONE CORE - BITMASK] Deallocazioni eseguite con successo.\n", MOD_NAME);

	        return 1;
        }

        /* 512 * 8 = 4096 BYTE */
        bitmask[index] = (uint64_t *)kzalloc(sizeof(uint64_t) * 512, GFP_KERNEL);

        if(bitmask[index] == NULL)
        {
            printk("%s: [ERRORE INIZIALIZZAZIONE CORE - BITMASK] Errore esecuzione kzalloc per la entry della bitmask %lld.\n", MOD_NAME, index);

            for(roll_index=0; roll_index<index; roll_index++)
            {
                kfree(bitmask[roll_index]);    
            }

            kfree(bitmask);

            bitmask = NULL;

            printk("%s: [ERRORE INIZIALIZZAZIONE CORE - BITMASK] Deallocazioni eseguite con successo.\n", MOD_NAME);

	        return 1;
        }

        block_state = (uint64_t *)bh->b_data;

        for(sub_index=0;sub_index<512;sub_index++)
        {
            bitmask[index][sub_index]= block_state[sub_index]; 
        }

        brelse(bh);

        printk("%s: [INIZIALIZZAZIONE CORE - BITMASK] Inizializzazione blocco bitmask #%lld è stata completata con successo.\n", MOD_NAME, index);
    }

    printk("%s: [INIZIALIZZAZIONE CORE - BITMASK] Inizializzazione bitmask completata con successo.\n", MOD_NAME);

    return 0;
    
}



/*
 * Questa funzione implementa un semplice controllo sulla
 * consistenza delle informazioni presenti nella lista dei
 * blocchi liberi rispetto alle informazioni mantenute nella
 * bitmask.
 */
void check_consistenza(void)
{
    struct block_free *bf;
    int bitmask_entry;
    int array_entry;
    uint64_t offset;
    uint64_t base;

    base = 1;

    bf = head_free_block_list;

    while(bf!=NULL)
    {
        printk("%s: [CHECK CONSISTENZA] valore indice del blocco inserito nella lista dei blocchi liberi è pari a %lld\n", MOD_NAME, bf->block_index);
    
        // Determino l'array di uint64_t */
        bitmask_entry = bf->block_index / (SOAFS_BLOCK_SIZE << 3);

        // Determino la entry dell'array */
        array_entry = (bf->block_index  % (SOAFS_BLOCK_SIZE << 3)) / (sizeof(uint64_t) * 8);

        offset = bf->block_index % (sizeof(uint64_t) * 8);

        if(bitmask[bitmask_entry][array_entry] & (base << offset))
        {
            printk("%s: [ERRORE CHECK CONSISTENZA] Errore inconsistenza per l'indice %lld.\n", MOD_NAME, bf->block_index);
        }
        else
        {
#ifdef NOT_CRITICAL
            printk("%s: [CHECK CONSISTENZA] Nessun errore di inconsistenza per l'indice %lld.\n", MOD_NAME, bf->block_index);
#endif
        }
        
        bf = bf->next;
    }
}



/*
 * Restituisce l'indice del blocco libero che si trova in testa.
 *  alla lista. Questa funzione può essere eseguita in concorrenza:
 * più thread stanno cercando di prendere un blocco per inserire
 * un nuovo messaggio.
 */
struct block_free * get_freelist_head(void)
{
    struct block_free *old_head;  
    int ret;
    int n; 

    n = 0;

retry_freelist_head:

#ifdef NOT_CRITICAL_PUT
    printk("%s: [PUT DATA - GET HEAD FREE LIST] Tentativo #%d di recupero del blocco in testa alla lista\n", MOD_NAME, n);
#endif

    old_head = head_free_block_list;

    /* Gestione di molteplici scritture concorrenti */
    if(old_head == NULL)
    {
#ifdef NOT_CRITICAL_BUT_PUT
        printk("%s: [ERRORE PUT DATA - GET HEAD FREE LIST] La lista risulta essere attualmente vuota al tentativo #%d\n", MOD_NAME, n);
#endif
        return NULL;
    }

    ret = __sync_bool_compare_and_swap(&head_free_block_list, old_head, head_free_block_list->next);

    if(!ret)
    {
        n++;

        if(n > 10)
        {
            printk("%s: [ERRORE PUT DATA - GET HEAD FREE LIST] Numero di tentativi massimo raggiunto per il recupero del blocco\n", MOD_NAME);
            return NULL;
        }

#ifdef NOT_CRITICAL_PUT
        printk("%s: ERRORE PUT DATA - GET HEAD FREE LIST Conflitto nel determinare il blocco\n", MOD_NAME);
#endif
    
        goto retry_freelist_head;
    }

    return old_head;
}



/*
 * Questa funzione ha il compito di verificare la validità
 * del blocco il cui indice è passato come parametro.
 */
int check_bit(uint64_t index)
{
    uint64_t base;
    uint64_t offset;
    int bitmask_entry;
    int array_entry;
    int bits;

    bits = sizeof(uint64_t) * 8;
    base = 1;

    /* 
     * Determino il blocco di stato che contiene
     * l'informazione relativa al blocco che sto
     * richiedendo (i.e., l'array di uint64_t).
     */
    bitmask_entry = index / (SOAFS_BLOCK_SIZE << 3);

    /* Determino la entry dell'array */
    array_entry = (index  % (SOAFS_BLOCK_SIZE << 3)) / bits;

    /* Determino l'offset nella entry dell'array */
    offset = index % bits;

    if(bitmask[bitmask_entry][array_entry] & (base << offset))
    {

#ifdef NOT_CRITICAL_INIT
        printk("%s: [CHECK BIT BITMASK] Il blocco di dati ad offset %lld è valido.\n", MOD_NAME, index);
#endif

        return 1;
    }

#ifdef NOT_CRITICAL_INIT
    printk("%s: [CHECK BIT BITMASK] Il blocco di dati ad offset %lld non è valido.\n", MOD_NAME, index);
#endif
    
    return 0;
}



/*
 * Questa funzione ha il compito di recuperare gli
 * indici dei blocchi liberi da poter utilizzare
 * per inserire i nuovi messaggi.
 */
int get_bitmask_block(void)
{
    int ret;
    uint64_t index;
    uint64_t count;
    uint64_t num_block_data;
    struct soafs_sb_info *sbi;
    struct block_free *old_head;
    struct block_free *bf;

    sbi = (struct soafs_sb_info *)sb_global->s_fs_info;

    /*
     * Devo gestire la concorrenza tra i vari scrittori. Può
     * accadere che più thread nello stesso momento
     * devono invocare questa funzione. Solamente un
     * thread alla volta può eseguirla in modo da
     * caricare nella lista le informazioni solamente
     * una volta. Dopo che il primo thread ha eseguito
     * questa funzione, i threads successivi possono
     * trovarsi la lista non vuota, poiché precedentemente
     * popolata, oppure la lista nuovamente vuota ma
     * con tutti i blocchi liberi già utilizzati.
     * In entrambi i casi, la funzione non deve essere
     * eseguita poiché o i blocchi sono stati già
     * recuperati oppure non esiste alcun blocco libero
     * da utilizzare per inserire il messaggio.
     */

    if( (head_free_block_list != NULL) || (num_block_free_used == sbi->num_block_free))
    {

#ifdef NOT_CRITICAL_BUT_PUT
        printk("%s: [PUT DATA - RECUPERO BLOCCHI] I blocchi sono stati già determinati o invalidati\n", MOD_NAME);
#endif

        return 0;
    }

    /* Recupero il numero totale dei blocchi di dati */
    num_block_data = sbi->num_block - 2 - sbi->num_block_state;

    /* 
     * Questa variabile rappresenta il numero massimo di
     * blocchi che è possibile inserire all'interno della
     * lista. In questo modo, riesco a gestire meglio la
     * quantità di memoria utilizzata. 
     */
    count = sbi->update_list_size;


    /*
     * Itero sui bit di stato alla ricerca dei
     * blocchi liberi da inserire nella lista.
     */
    for(index = pos; index<num_block_data; index++)
    {
#ifdef NOT_CRITICAL_PUT
        printk("%s: [PUT DATA - RECUPERO BLOCCHI] Verifica della validità del blocco con indice %lld\n", MOD_NAME, index);
#endif
        ret = check_bit(index);

        if(!ret)
        {

            /* Ho trovato un nuovo blocco libero da inserire nella free list */

            bf = (struct block_free *)kmalloc(sizeof(struct block_free), GFP_KERNEL);

            if(bf == NULL)
            {
                    printk("%s: [ERRORE PUT DATA - RECUPERO BLOCCHI] Errore esecuzione della kmalloc()\n", MOD_NAME);
                    return 1;
            }

            bf -> block_index = index;
retry:

            /* Implemento un inserimento in testa alla lista */
            old_head = head_free_block_list;
        
            bf->next = head_free_block_list;

            ret = __sync_bool_compare_and_swap(&head_free_block_list, old_head, bf);

            if(!ret)
            {

#ifdef NOT_CRITICAL_BUT_PUT
                    printk("%s: [ERRORE PUT DATA - RECUPERO BLOCCHI] Errore inserimento del blocco in concorrenza\n", MOD_NAME);
#endif

                    goto retry;
            }

            pos = index + 1;

#ifdef NOT_CRITICAL_PUT
            printk("%s: [PUT DATA - RECUPERO BLOCCHI] Il nuovo valore di pos è pari a %lld\n", MOD_NAME, pos);
#endif

            num_block_free_used++;

            asm volatile ("mfence");

            count --;

            /*
             * Verifico se ho esaurito il numero di blocchi liberi oppure
             * se ho già caricato il numero massimo di blocchi all'interno
             * della lista. In entrambi i casi, interrompo la ricerca
             * in modo da risparmiare le risorse.
             */
            if( (num_block_free_used == sbi->num_block_free)  || (count == 0) )
            {

#ifdef NOT_CRITICAL_BUT_PUT
                if(num_block_free_used == sbi->num_block_free)
                {
                    printk("%s: [PUT DATA - RECUPERO BLOCCHI] Ho esaurito il numero di blocchi liberi totali\n", MOD_NAME);
                }
                else
                {
                    printk("%s: [PUT DATA - RECUPERO BLOCCHI] Ho inserito il numero massimo di elementi all'interno della lista\n", MOD_NAME);
                }
#endif

                break;  
            }                       
        }
    
    }

    return 0;
    
}



/*
 * Inserisce un nuovo elemento all'interno della lista free_block_list.
 * L'inserimento viene fatto in testa poiché non mi importa mantenere
 * alcun ordine tra i blocchi liberi. Questa funzione non viene eseguita
 * in concorrenza.
 */
int insert_free_list(uint64_t index)
{
    struct block_free *new_item;
    struct block_free *old_head;

    new_item = (struct block_free *)kmalloc(sizeof(struct block_free), GFP_KERNEL);

    if(new_item==NULL)
    {
        printk("%s: [ERRORE INIZIALIZZAZIONE CORE - FREE LIST] Errore malloc() free list.", MOD_NAME);
        return 1;
    }

    new_item->block_index = index;

    if(head_free_block_list == NULL)
    {
        head_free_block_list = new_item;
        head_free_block_list -> next = NULL;
    }
    else
    {
        old_head = head_free_block_list;
        head_free_block_list = new_item;
        head_free_block_list -> next = old_head;
    }

#ifdef NOT_CRITICAL_INIT
    printk("%s: [INIZIALIZZAZIONE CORE - FREE LIST] Inserito il blocco %lld nella lista dei blocchi liberi.\n", MOD_NAME, index);
#endif

    return 0;
}



/*
 * index_free: array con indici dei blocchi liberi
 * actual_size: dimensione effettiva dell'array
 *
 * Questa funzione ha il compito di inizializzare la lista contenente
 * gli indici dei blocchi liberi.
 */
int init_free_block_list(uint64_t *index_free, uint64_t actual_size) //
{
    uint64_t index;
    uint64_t roll_index;
    int ret;
    struct block_free *roll_bf;

    if(SIZE_INIT < actual_size)
    {
        printk("%s: [ERRORE INIZIALIZZAZIONE CORE - FREE LIST]  Errore nella dimensione dell'array.\nACTUAL_SIZE = %lld\tSIZE_INIT = %d\n", MOD_NAME, actual_size, SIZE_INIT);

        return 1;
    }

    for(index=0; index<actual_size;index++)
    {

#ifdef NOT_CRITICAL_INIT
        printk("%s: [INIZIALIZZAZIONE CORE - FREE LIST] Inserimento del blocco %lld all'interno della lista in corso...\n", MOD_NAME, index_free[index]);
#endif

        ret = insert_free_list(index_free[index]);

        if(ret)
        {
            printk("%s: [ERRORE INIZIALIZZAZIONE CORE - FREE LIST] Errore kzalloc() free_list indice %lld.\n", MOD_NAME, index);

            for(roll_index=0; roll_index<index;roll_index++)
            {
                roll_bf = head_free_block_list->next;

                kfree(head_free_block_list);

                head_free_block_list = roll_bf;
            }

            printk("%s: [ERRORE INIZIALIZZAZIONE CORE - FREE LIST] Rollback eseguito con successo...\n", MOD_NAME);

            return 1;
        }

#ifdef NOT_CRITICAL_INIT
        printk("%s: [INIZIALIZZAZIONE CORE - FREE LIST] Blocco #%lld inserito correttamente all'interno della lista\n", MOD_NAME, index_free[index]); 
#endif    
   
    }

    num_block_free_used = actual_size;

    pos = index_free[actual_size - 1] + 1;

    printk("%s: [INIZIALIZZAZIONE CORE - FREE LIST] Il numero di blocchi liberi utilizzati è pari a %lld.\n", MOD_NAME, num_block_free_used);

    printk("%s: [INIZIALIZZAZIONE CORE - FREE LIST] Il valore di pos è pari a %lld.\n", MOD_NAME, pos);

    check_consistenza();

    return 0;
}



/*
 * Questa funzione mi consente di modificare le informazioni
 * di validità dei blocchi all'interno della bitmask.
 */
void set_bitmask(uint64_t index, int mode)
{
    uint64_t base;
    uint64_t shift_base;
    uint64_t offset;
    int bitmask_entry;
    int array_entry;
    int bits;

    bits = sizeof(uint64_t) * 8;
    

    /* 
     * Determino il blocco di stato che contiene
     * l'informazione relativa al blocco che sto
     * richiedendo (i.e., l'array di uint64_t).
     */
    bitmask_entry = index / (SOAFS_BLOCK_SIZE << 3);

    /* Determino la entry dell'array */
    array_entry = (index  % (SOAFS_BLOCK_SIZE << 3)) / bits;

    /* Determino l'offset nella entry dell'array */
    offset = index % bits;
    
    base = 1;

    shift_base = base << offset;

    if(mode)
    {
        __sync_fetch_and_or(&(bitmask[bitmask_entry][array_entry]), shift_base);
    }
    else
    {
        __sync_fetch_and_xor(&(bitmask[bitmask_entry][array_entry]), shift_base);
    }   

}


/*
 * Questa funzione è simile alla funzione 'insert_sorted_list'
 * che si trova successivamente. La differenza con l'altra funzione
 * sta nel fatto che questa gestisce la concorrenza.
 * In questo scenario, l'inserimento dei blocchi all'interno
 * della lista viene fatto in coda.
 */
int insert_sorted_list_conc(struct block *block)
{
    struct block *next;
    struct block *curr;
    int ret;
    int n;

    n = 0;

    block->sorted_list_next = NULL;

    next = NULL;

    if(head_sorted_list == NULL)
    {
        ret = __sync_bool_compare_and_swap(&head_sorted_list, next, block);

        if(!ret)
        {
#ifdef NOT_CRITICAL_BUT
            printk("%s: [ERRORE PUT DATA - INSERIMENTO HT + SORTED] L'inserimento in coda non è stato eseguito poiché la lista non è più vuota\n", MOD_NAME);
#endif
            n++;
            goto no_empty;        
        }

#ifdef NOT_CRITICAL_BUT
        printk("%s: [PUT DATA - INSERIMENTO HT + SORTED] Inserimento in coda nella lista ordinata effettuato con successo per il blocco %lld\n", MOD_NAME, block->block_index);
#endif    
        return 0;
    }

no_empty:

    /*
     * Poiché le invalidazioni dei blocchi non possono essere eseguite
     * in concorrenza con gli inserimenti, da questo momento la lista
     * non potrà essere vuota. In questo modo, posso gestire correttamente
     * il puntatore 'next'.
     */

    if(n > 10)
    {
        printk("%s: [ERRORE PUT DATA - INSERIMENTO HT + SORTED] Numero tentativi massimo raggiunto per l'inserimento nella sorted list del blocco %lld\n", MOD_NAME, block->block_index);
        return 1;
    }

    curr = head_sorted_list;

    while( curr->sorted_list_next != NULL )
    {
        curr = curr->sorted_list_next;
    }

    ret = __sync_bool_compare_and_swap(&(curr->sorted_list_next), next, block);

    if(!ret)
    {
#ifdef NOT_CRITICAL_BUT
        printk("%s: [ERRORE PUT DATA - INSERIMENTO HT + SORTED] Tentativo di inserimento #%d del blocco %lld terminato senza successo\n", MOD_NAME, n, block->block_index);
#endif
        n++;
        goto no_empty;
    }

#ifdef NOT_CRITICAL_BUT
    printk("%s: [PUT DATA - INSERIMENTO HT + SORTED] Inserimento in coda nella lista ordinata effettuato con successo per il blocco %lld al tentativo %d\n", MOD_NAME, block->block_index, n);
#endif

    return 0;    
}


/*
 * Inserisce un nuovo elemento all'interno della
 * lista contenente i blocchi ordinati secondo
 * l'ordine di consegna. L'elemento non deve essere
 * nuovamente allocato ma si collega l'elemento che 
 * è stato precedentemente allocato tramite puntatori.
 * Il campo 'pos' rappresenta la posizione del blocco
 * nella lista ordinata e viene sfruttato per determinare
 * la corretta posizione all'interno della lista.
 * Osservo che l'esecuzione di questa funzione avviene in
 * assenza di concorrenza.
 */
void insert_sorted_list(struct block *block)
{
    struct block *prev;
    struct block *curr;

    if(head_sorted_list == NULL)
    {
        /* La lista è vuota ed eseguo un inserimento in testa */
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
            /* Cerco la posizione per inserire il nuovo elemento */

            /* L'elemento che dovrà precedere il nuovo item */
            prev = head_sorted_list;

            /* L'elemento che dovrà seguire il nuovo item */
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



void check_rollback(uint64_t index, struct ht_valid_entry * ht_entry)
{
    struct block *curr;

    curr = ht_entry->head_list;

    while(curr != NULL)
    {
        if(curr->block_index == index)
        {
            printk("%s: [ERRORE CHECK ROLLBACK] La procedura di rollback non è stata eseguita con successo per il blocco con indice %lld\n", MOD_NAME, index);
        }
        curr = curr->hash_table_next;
    }

    printk("%s: [CHECK ROLLBACK] La procedura di rollback è stata eseguita con successo per il blocco con indice %lld\n", MOD_NAME, index);
}



/*
 * Esegue la procedura di rollback relativa all'inserimento di un
 * nuovo blocco. Più precisamente, rimuove il blocco dalla
 * lista corrispondente nella hash table.
 */
static void rollback(uint64_t index, struct ht_valid_entry * ht_entry)
{

    int ret;
    int n;
    struct block *curr;
    struct block *prev;
    

    curr = ht_entry->head_list;

    /*
     * Poiché durante la fase di rollback non è possibile
     * avere invalidazioni e gli inserimenti nelle
     * liste della HT avvengono in testa, allora non dovrebbe
     * esserci alcun nuovo blocco tra quello che si deve
     * eliminare e il successivo (se esiste).
     */



    /*
     * Verifico se il blocco corrisponde alla testa della lista.
     * In questo caso, nessun altro blocco è stato successivamente
     * inserito.
     */

    if(curr->block_index == index)
    {
        ret = __sync_bool_compare_and_swap(&(ht_entry->head_list), curr, (ht_entry->head_list)->hash_table_next);

        if(!ret)
        {
            /* Sono stati inseriti nuovi blocchi in testa alla lista */
            goto no_head;
        }

        printk("%s: [PUT DATA - ROLLBACK] Rollback completato con successo: blocco %lld eliminato dalla HT\n", MOD_NAME, index);

        kfree(curr);

        check_rollback(index, ht_entry);

        return ;
    }

    n = 0;

no_head:

    prev = ht_entry->head_list;

    curr = prev->hash_table_next;

    while(curr != NULL)
    {
        if( curr->block_index == index )
        {
            break;
        }

        prev = curr;
        curr = curr->hash_table_next;
    }

    if(curr == NULL)
    {
        printk("%s: [ERRORE PUT DATA - ROLLBACK] Il blocco %lld da rimuovere non è presente nella lista\n", MOD_NAME, index);
        return ;
    }

    ret = __sync_bool_compare_and_swap(&(prev->hash_table_next), curr, curr->hash_table_next);

    if(!ret)
    {
        n++;

#ifdef NOT_CRITICAL_BUT
        printk("%s: [ERRORE PUT DATA - ROLLBACK] Tentativo #%d fallito nell'esecuzione della procedura di rollback\n", MOD_NAME, n);
#endif

        goto no_head;
    }

    printk("%s: [PUT DATA - ROLLBACK] Rollback completato con successo: blocco %lld eliminato dalla HT\n", MOD_NAME, index);

    kfree(curr);
}



/*
 * Questa funzione ha il compito di inserire l'indice del blocco libero
 * all'interno della lista poiché l'inserimento del blocco non è avvenuto
 * con successo.
 */
void rollback_insert_ht_sorted(struct block_free *item)
{

    insert_free_list_conc(item);

    printk("%s: [ERRORE PUT DATA - INSERIMENTO HT + SORTED] Rollback eseguito con successo\n", MOD_NAME);    
}



/*
 * Inserisce un nuovo elemento all'interno della lista
 * nella hash table e all'interno della lista dei blocchi
 * ordinati. Questa funzione viene eseguita in concorrenza.
 */
int insert_hash_table_valid_and_sorted_list_conc(char *data_block_msg, uint64_t pos, uint64_t index, struct block_free *item)
{
    int num_entry_ht;
    int ret;
    int n;                                      /* Contatore del numero di tentativi effettuati */
    struct block *new_item;
    struct block *old_head;
    struct ht_valid_entry *ht_entry; 

    /* Identifico la lista corretta nella hash table per effettuare l'inserimento */
    num_entry_ht = index % x;

    ht_entry = &(hash_table_valid[num_entry_ht]);

    /* Alloco il nuovo elemento da inserire nelle liste */
    new_item = (struct block *)kmalloc(sizeof(struct block), GFP_KERNEL);
    
    if(new_item == NULL)
    {
        printk("%s: [ERRORE PUT DATA - INSERIMENTO HT + SORTED] Errore esecuzione della kmalloc()\n", MOD_NAME);
        return 1;
    }

    /* Inizializzo il nuovo elemento */
    new_item->block_index = index;

    new_item->pos = pos;

    new_item->msg = data_block_msg;

    /* Gestisco la concorrenza con le invalidazioni */

    n = 0;

retry_mutex_inval_insert:

    if(n > 10)
    {
        printk("%s: [ERRORE PUT DATA - INSERIMENTO HT + SORTED] Il numero massimo di tentativi %d per il blocco %lld è stato raggiunto\n", MOD_NAME, n, index);
        return 1;
    }

    mutex_lock(&inval_insert_mutex);    

    if( sync_var & MASK_INVALIDATE )
    {
#ifdef NOT_CRITICAL_BUT
        printk("%s: [ERRORE PUT DATA - INSERIMENTO HT + SORTED] (%d) E' in corso un'invalidazione, attendere...\n", MOD_NAME, n);
#endif
        mutex_unlock(&inval_insert_mutex);

        wait_event_interruptible(the_queue, (sync_var & MASK_INVALIDATE) == 0 );

        n++;

        goto retry_mutex_inval_insert;
    }

    /* Comunico la presenza del thread che effettuerà l'inserimento di un nuovo blocco */
    __sync_fetch_and_add(&sync_var,1);

#ifdef NOT_CRITICAL_BUT
    printk("%s: [PUT DATA - INSERIMENTO HT + SORTED] Segnalata la presenza per l'inserimento del blocco %lld\n", MOD_NAME, index);
#endif

    mutex_unlock(&inval_insert_mutex);

    /* Inserimento in testa nella lista della HT */

    n = 0;

retry_insert_ht:

    if(n > 10)
    {
        printk("%s: [ERRORE PUT DATA - INSERIMENTO HT + SORTED] (%d) Il numero di tentativi massimo raggiunto per l'inserimento nella HT\n", MOD_NAME, n);

        __sync_fetch_and_sub(&sync_var,1);
    
        rollback_insert_ht_sorted(item);

        /*
         * In questo modo, sveglio i thread che sono in attesa
         * della conclusione degli inserimenti. Se gli inserimenti
         * sono effettivamente conclusi, allora possono procedere con
         * l'invalidazione del blocco richiesto.
         */
        wake_up_interruptible(&the_queue);

        return 1;
    }

    old_head = ht_entry->head_list;

    new_item->hash_table_next = old_head;

    ret = __sync_bool_compare_and_swap(&(ht_entry->head_list), old_head, new_item);

    if(!ret)
    {
#ifdef NOT_CRITICAL_BUT
        printk("%s: [ERRORE PUT DATA - INSERIMENTO HT + SORTED] Conflitto inserimento in testa nella lista #%d della HT\n", MOD_NAME, num_entry_ht);
#endif
        n++;
        goto retry_insert_ht;
    }

#ifdef NOT_CRITICAL
    printk("%s: [PUT DATA - INSERIMENTO HT + SORTED] Inserimento blocco %lld nella entry #%d della HT completato con successo.\n", MOD_NAME, index, num_entry_ht);
#endif

    ret = insert_sorted_list_conc(new_item);            /* Inserimento del blocco nella lista ordinata */

    if(ret)
    {
        rollback(new_item->block_index, ht_entry);

        __sync_fetch_and_sub(&sync_var,1);

        rollback_insert_ht_sorted(item);

        wake_up_interruptible(&the_queue);

        return 1;        
    }

    __sync_fetch_and_sub(&sync_var,1);                  /* Segnalo che l'operazione di inserimento si è conclusa */

    wake_up_interruptible(&the_queue);

#ifdef NOT_CRITICAL
    printk("%s: [PUT DATA - INSERIMENTO HT + SORTED] Inserimento blocco %lld nella sorted list avvenuto con successo\n", MOD_NAME, index);
#endif

    return 0;    
}




/*
 * Inserisce un nuovo elemento all'interno della lista
 * corretta nella HT e nella lista ordinata.
 */
static int insert_hash_table_valid_and_sorted_list(char *data_block_msg, uint64_t pos, uint64_t index)
{
    int num_entry_ht;
    size_t len;
    struct block *new_item;
    struct block *old_head;
    struct ht_valid_entry *ht_entry; 

    /* Identifico la lista corretta nella hash table per effettuare l'inserimento */
    num_entry_ht = index % x;

    ht_entry = &(hash_table_valid[num_entry_ht]);

    /* Alloco il nuovo elemento da inserire nella lista */
    new_item = (struct block *)kmalloc(sizeof(struct block), GFP_KERNEL);
    
    if(new_item == NULL)
    {
        printk("%s: [ERRORE INIZIALIZZAZIONE CORE - HT + SORTED] Errore malloc() nell'allocazione del nuovo elemento da inserire nella hash table.", MOD_NAME);
        return 1;
    }

    /* Inizializzo il nuovo elemento */
    new_item->block_index = index;

    new_item->pos = pos;

    len = strlen(data_block_msg) + 1;

    new_item->msg = (char *)kmalloc(len, GFP_KERNEL);

    if(new_item->msg == NULL)
    {
        printk("%s: [ERRORE INIZIALIZZAZIONE CORE - HT + SORTED] Errore malloc() nell'allocazione della memoria per il messaggio dell'elemento da inserire nella hash table.", MOD_NAME);
        return 1;
    }

    strncpy(new_item->msg, data_block_msg, len);

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

#ifdef NOT_CRITICAL_INIT
    printk("%s: [INIZIALIZZAZIONE CORE - HT + SORTED] Inserimento blocco %lld nella entry #%d completato con successo.\n", MOD_NAME, index, num_entry_ht);
#endif

    /* Inserimento del blocco nella lista ordinata */
    insert_sorted_list(new_item);

    return 0;   
    
}




int init_ht_valid_and_sorted_list(uint64_t num_data_block)
{
    uint64_t index;
    int isValid;
    int ret;
    struct buffer_head *bh;
    struct soafs_block *data_block;
    struct soafs_sb_info *sbi;

    sbi = (struct soafs_sb_info *)sb_global->s_fs_info;

    if(sbi->num_block_free > sbi->num_block)
    {
        printk("%s: [ERRORE INIZIALIZZAZIONE CORE - HT + SORTED] Numero di blocchi liberi maggiore del numero dei blocchi totale\n", MOD_NAME);

        return 1;
    }
    
    if(sbi->num_block_free == sbi->num_block)
    {
        printk("%s: [INIZIALIZZAZIONE CORE - HT + SORTED] Non ci sono blocchi attualmente validi\n", MOD_NAME);

        head_sorted_list = NULL;

        return 0;
    }

    bh = NULL;
    data_block = NULL;

    for(index=0; index<num_data_block; index++)
    {
        /* Verifico se il blocco è valido */
        isValid = check_bit(index);

        if(!isValid)
        {
            continue;
        }

        bh = sb_bread(sb_global, 2 + sbi->num_block_state + index);                   

        if(bh == NULL)
        {
            printk("%s: [ERRORE INIZIALIZZAZIONE CORE - HT + SORTED] Errore esecuzione della sb_bread() per la lettura del blocco di dati con indice %lld...\n", MOD_NAME, index);
            return 1;
        }

        data_block = (struct soafs_block *)bh->b_data;

        ret = insert_hash_table_valid_and_sorted_list(data_block->msg, data_block->pos, index);

        if(ret)
        {
            printk("%s: [ERRORE INIZIALIZZAZIONE CORE - HT + SORTED] Errore inserimento nella HT del blocco con indice %lld.\n", MOD_NAME, index);
            return 1;
        }

#ifdef NOT_CRITICAL_BUT_INIT
        printk("%s: [INIZIALIZZAZIONE CORE - HT + SORTED] Il blocco di dati con indice %lld è valido e nella lista ordinata si trova in posizione %lld.\n", MOD_NAME, index, data_block->pos);
#endif
        brelse(bh);        
        
    }

    return 0;
}



int init_data_structure_core(uint64_t num_data_block, uint64_t *index_free, uint64_t actual_size) //
{
    int ret;
    int i;
    uint64_t index;
    size_t size_ht;
    struct soafs_sb_info *sbi;
    struct block_free *roll_bf;

    if(sb_global == NULL)
    {
        printk("%s: [ERRORE INIZIALIZZAZIONE CORE] Il contenuto del superblocco non è valido. Impossibile inizializzare le strutture dati core.\n", MOD_NAME);

        is_free = 1;

        return 1;
    }

    if(num_data_block <= 0)
    {
        printk("%s: [ERRORE INIZIALIZZAZIONE CORE] Il numero dei blocchi di dati del device non è valido. Impossibile inizializzare le strutture dati core.\n", MOD_NAME);

        is_free = 1;

        return 1;
    }

    /* Inizializzo la bitmask */
    ret = init_bitmask();

    if(ret)
    {
        printk("%s: [ERRORE INIZIALIZZAZIONE CORE] Errore inizializzazione bitmask, non è possibile completare l'inizializzazione core.\n", MOD_NAME);

        is_free = 1;

        return 1;
    }

    sbi = (struct soafs_sb_info *)sb_global->s_fs_info;

    printk("%s: [INIZIALIZZAZIONE CORE] Valore di 'actual_size' è pari a %lld\n", MOD_NAME, actual_size);

    if( (actual_size < 0) || (sbi->num_block_free < 0) || ((actual_size > 0) && (sbi->num_block_free == 0)) )
    {
        printk("%s: [ERRORE INIZIALIZZAZIONE CORE] Le informazioni sui blocchi liberi non sono valide.\n", MOD_NAME);

        for(index=0;index<sbi->num_block_state;index++)
        {
            kfree(bitmask[index]);        
        }

        kfree(bitmask);

        bitmask = NULL;

        printk("%s: [ERRORE INIZIALIZZAZIONE CORE] Deallocazione bitmask completata con successo...\n", MOD_NAME);

        is_free = 1;

        return 1;
    }


    /* Inizializzo la free_block_list */
    if(actual_size > 0)
    {
        ret = init_free_block_list(index_free, actual_size);

        if(ret)
        {
            printk("%s: [ERRORE INIZIALIZZAZIONE CORE] Errore inizializzazione free_block_list, non è possibile completare l'inizializzazione core.\n", MOD_NAME);
    
            for(index=0;index<sbi->num_block_state;index++)
            {
                kfree(bitmask[index]);        
            }

            kfree(bitmask);

            bitmask = NULL;

            printk("%s: [ERRORE INIZIALIZZAZIONE CORE] Deallocazione bitmask completata con successo...\n", MOD_NAME);

            is_free = 1;

            return 1;
        }

    }else
    {
        num_block_free_used = 0;

        pos = 0;

        head_free_block_list = NULL;
    }


    /* Inizializzazione HT e sorted_list */
    compute_num_rows(num_data_block);

    size_ht = x * sizeof(struct ht_valid_entry);

    hash_table_valid = (struct ht_valid_entry *)kmalloc(size_ht, GFP_KERNEL);

    if(hash_table_valid == NULL)
    {
        printk("%s: [ERRORE INIZIALIZZAZIONE CORE] Errore esecuzione kmalloc() nell'allocazione della memoria per la tabella hash.\n", MOD_NAME);

        for(index=0;index<sbi->num_block_state;index++)
        {   
            kfree(bitmask[index]);        
        }

        kfree(bitmask);

        bitmask = NULL;

        printk("%s: [ERRORE INIZIALIZZAZIONE CORE] Deallocazione bitmask completata con successo...\n", MOD_NAME);

        for(index=0; index<actual_size; index++)
        {
            roll_bf = head_free_block_list->next;

            kfree(head_free_block_list);

            head_free_block_list = roll_bf;
        }

        if(head_free_block_list != NULL)
        {
            printk("%s: [ERRORE INIZIALIZZAZIONE CORE] Errore nella deallocazione della FREE LIST... La lista non è vuota al termine della deallocazione\n", MOD_NAME);
        }
        else
        {
            printk("%s: [ERRORE INIZIALIZZAZIONE CORE] Deallocazione della FREE LIST completata con successo...\n", MOD_NAME);
        }

        is_free = 1;

        return 1;
    }

    for(index=0;index<x;index++)
    {
        (&hash_table_valid[index])->head_list = NULL;
    }
    
    ret = init_ht_valid_and_sorted_list(num_data_block);

    if(ret)
    {
        printk("%s: [ERRORE INIZIALIZZAZIONE CORE] Errore inizializzazione HT e sorted_list\n", MOD_NAME);

        for(index=0;index<sbi->num_block_state;index++)
        {   
            kfree(bitmask[index]);        
        }

        kfree(bitmask);

        bitmask = NULL;

        printk("%s: [ERRORE INIZIALIZZAZIONE CORE] Deallocazione bitmask completata con successo...\n", MOD_NAME);

        for(index=0; index<actual_size; index++)
        {
            roll_bf = head_free_block_list->next;

            kfree(head_free_block_list);

            head_free_block_list = roll_bf;
        }

        if(head_free_block_list != NULL)
        {
            printk("%s: [ERRORE INIZIALIZZAZIONE CORE] Errore nella deallocazione della FREE LIST... La lista non è vuota al termine della deallocazione\n", MOD_NAME);
        }
        else
        {
            printk("%s: [ERRORE INIZIALIZZAZIONE CORE] Deallocazione della FREE LIST completata con successo...\n", MOD_NAME);
        }
    
        is_free = 1;

        return 1;
    }

    gp = (struct grace_period *)kmalloc(sizeof(struct grace_period), GFP_KERNEL);

    if(gp == NULL)
    {
        printk("%s: [ERRORE INIZIALIZZAZIONE CORE] Errore inizializzazione grace period\n", MOD_NAME);

        free_all_memory();

        is_free = 1;

        return 1;
    }

    gp->epoch_ht = 0x0;
    gp->epoch_sorted = 0x0;

    gp->next_epoch_index_ht = 0x1;
    gp->next_epoch_index_sorted = 0x1;

    for(i=0;i<EPOCHS;i++)
    {
        gp->standing_ht[i] = 0x0;
        gp->standing_sorted[i] = 0x0;
    }

    //debugging_init();

    return 0;
}
