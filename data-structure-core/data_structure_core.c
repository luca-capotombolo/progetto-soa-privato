#include <linux/buffer_head.h>  /* sb_bread()-brelse() */
#include <linux/string.h>       /* strncpy() */
#include <linux/slab.h>         /* kmalloc() */
#include <linux/wait.h>         /* wait_event_interruptible() - wake_up_interruptible() */
#include <linux/jiffies.h>

#include "../headers/main_header.h"



struct block_free *head_free_block_list = NULL;     /* Puntatore alla testa della lista contenente i blocchi liberi */
struct grace_period *gp = NULL;                     /* Strtuttura dati per il Grace Period */

uint64_t num_block_free_used = 0;                   /* Numero di blocchi liberi al tempo di montaggio caricati nella lista */
uint64_t pos = 0;                                   /* Indice da cui iniziare la ricerca dei nuovi blocchi liberi */
uint64_t sync_var = 0;                              /* Variabile per la sincronizzazione inserimenti-invalidazioni */




/**
 * scan_free_list - Esegue la scansione della struttura dati free_block_list
 *
 * Questa funzione ha prevalentemente uno scopo per il debugging del modulo
 * 
 * @returns: La funzione non restituisce alcun valore.
 */
static void scan_free_list(void)
{
    struct block_free *curr;

    curr = head_free_block_list;

    printk("%s: ------------------------------ INIZIO FREE LIST ---------------------------------------", MOD_NAME);

    while(curr!=NULL)
    {
        printk("Blocco #%lld\n", curr->block_index);
        curr = curr->next;
    }

    printk("%s: -------------------------------  FINE FREE LIST  ---------------------------------------\n\n", MOD_NAME);
}




/**
 * debugging_init - Esegue la scansione delle strutture dati core del modulo
 *
 * Questa funzione ha prevalentemente uno scopo per il debugging del modulo
 * 
 * @returns: La funzione non restituisce alcun valore.
 */
static void debugging_init(void)
{
    /* scansione della lista contenente le informazioni dei blocchi liberi. */
    scan_free_list();
}




/**
 * get_block: Restituisce il puntatore superblocco del dispositivo
 *
 * @returns: Restituisce il puntatore alla struttura dati buffer_head tramite cui è possibile
 *           raggiungere il superblocco.
 */
struct buffer_head *get_sb_block(void)
{
    struct buffer_head *bh;

    bh = sb_bread(sb_global, SOAFS_SB_BLOCK_NUMBER);

    if(bh == NULL)
    {
        printk("%s: [ERRORE PUT DATA] Si è verificato un errore nella lettura del superblocco\n", MOD_NAME);
        return NULL;
    }

    return bh;
}




/**
 * get_block: Restituisce il puntatore al blocco del dispositivo con l'indice richiesto
 *
 * @index: Indice del blocco del dispositivo che deve essere recuperato
 *
 * @returns: Restituisce il puntatore alla struttura dati buffer_head tramite cui è possibile
 *           raggiungere il blocco richiesto.
 */
struct buffer_head *get_block(uint64_t index)
{
    struct buffer_head *bh;
    struct soafs_sb_info *sbi;

    /* Recupero le informazioni FS specific */
    sbi = (struct soafs_sb_info *)sb_global->s_fs_info;

    bh = sb_bread(sb_global, 2 + sbi->num_block_state + index);

    if(bh == NULL)
    {
        printk("%s: [ERRORE PUT DATA] Si è verificato un errore nella lettura del blocco %lld\n", MOD_NAME, index);
        return NULL;
    }

    return bh;
}




/**
 * scan_sorted_list - Esegue la scansione della Sorted List logica
 *
 * Questa funzione ha prevelentemente una funzione di debugging del modulo. Mi consente di osservare l'ordine
 * degli elementi che sono presenti all'interno della Sorted List.
 *
 * @returns: La funzione non restituisce alcun valore.
 */
void scan_sorted_list(void)
{
    uint64_t curr_index;

    struct soafs_block *b_data;
    struct soafs_super_block *b_data_sb;

    struct buffer_head *bh_sb;
    struct buffer_head *bh_b;

    struct soafs_sb_info *sbi;

    /* Recupero le informazioni FS specific */
    sbi = (struct soafs_sb_info *)sb_global->s_fs_info;

    /* Recupero il puntatore al superblocco del dispositivo che contiene l'indice del blocco in testa alla Sorted List */
    bh_sb = get_sb_block();

    if(bh_sb == NULL)
    {
        printk("%s: [ERRORE SCANSIONE SORTED LIST] Errore lettura del superblocco\n", MOD_NAME);
        return;
    }

    b_data_sb = (struct soafs_super_block *)bh_sb->b_data;

    if(b_data_sb == NULL)
    {
        printk("%s: [ERRORE SCANSIONE SORTED LIST] Errore puntatore a NULL per il superblocco\n", MOD_NAME);
        return;
    }

    /* Recupero l'indice del blocco in testa alla Sorted List che è contenuto nel superblocco del dispositivo */
    curr_index = b_data_sb->head_sorted_list;

    /* Recupero il puntatore al blocco del dispositivo che rappresenta la testa della Sorted List */
    bh_b = get_block(curr_index);

    if(bh_b == NULL)
    {
        printk("%s: [ERRORE SCANSIONE SORTED LIST] Errore nel recupero del blocco in testa alla lista\n", MOD_NAME);

        if(bh_sb != NULL)
            brelse(bh_sb);

        return;
    }

    b_data = (struct soafs_block *)bh_b -> b_data;

    if(b_data == NULL)
    {
        printk("%s: [ERRORE SCANSIONE SORTED LIST] Errore puntatore a NULL per il blocco in testa alla lista\n", MOD_NAME);
        return;
    }

    printk("---------------------------------------- INIZIO ELEMENTI DELLA SORTED LIST --------------------------------------\n");

    printk("%lld\n", curr_index);

    /* Eseguo la scansione di tutti i blocchi all'interno della Sorted List. Itero fino a quando non è l'ultimo item */

    while(b_data->next != sbi->num_block)
    {
        curr_index = b_data->next;

        printk("%lld\n", curr_index);

        if(bh_b != NULL)
            brelse(bh_b);

        bh_b = get_block(curr_index);

        if(bh_b == NULL)
        {
            printk("%s: [SCANSIONE] Errore nella lettura del blocco %lld durante la scansione della Sorted List\n", MOD_NAME, curr_index);

            if(bh_sb != NULL)
                brelse(bh_sb);
            
            return;
        }

        b_data = (struct soafs_block *)bh_b -> b_data;
    }

    printk("--------------------------------------------------------------------------------------------------------------\n\n");

    if(bh_b != NULL)
        brelse(bh_b);

    if(bh_sb != NULL)
        brelse(bh_sb);

    return;
}




/**
 * insert_new_data_block - Inserisce un nuovo blocco all'interno della Sorted List e lo rende valido
 *
 * @index: Indice del nuovo blocco da inserire nella Sorted List e da invalidare
 * @source: Puntatore al messaggio utente
 * @msg_size: Dimensione del messaggio da inserire all'interno del blocco
 *
 * Esegue l'inserimento del nuovo blocco con indice 'index' all'interno della Sorted List. L'inserimento
 * viene eseguito in coda alla lista ordinata per rispettare l'ordine di consegna richiesto.
 *
 * @returns: La funzione restituisce il valore 0 in caso di successo; altrimenti restituisce il valore 1.
 */
int insert_new_data_block(uint64_t index, char * source, size_t msg_size)
{
    int n;
    int count;
    int ret_cmp;
    int bytes_ret;
    uint64_t next_block_index;

    struct soafs_block *b_data;
    struct soafs_block *b_data_x;
    struct soafs_super_block *b_data_sb;

    struct buffer_head *bh_sb;
    struct buffer_head *bh_x;
    struct buffer_head *bh_b;
    struct soafs_sb_info *sbi;

    /* Recupero le informazioni FS specific */
    sbi = (struct soafs_sb_info *)sb_global->s_fs_info;

    /* Gestione della concorrenza con le invalidazioni */

    /* Inizializzo il numero di tentativi */
    n = 0;

retry_mutex_inval_insert:

    /*
     * Decido di implementare un meccanismo di retry che consente un numero massimo di tentativi. Il numero massimo
     * di tentativi permette di evitare lo scenario in cui il thread rimane bloccato per troppo tempo. Sarà poi
     * l'utente a decidere se richiedere successivamente l'inserimento dopo il verificarsi dell'errore.
     */

    if(n > 20)
    {
        printk("%s: [ERRORE PUT DATA - SORTED LIST] Il numero massimo di tentativi %d per il blocco %lld è stato raggiunto\n", MOD_NAME, n, index);
        return 1;
    }

    mutex_lock(&inval_insert_mutex);    

    if( sync_var & MASK_INVALIDATE )
    {
#ifdef NOT_CRITICAL_BUT_PUT
        printk("%s: [ERRORE PUT DATA - SORTED LIST] (%d) E' in corso un'invalidazione, attendere...\n", MOD_NAME, n);
#endif
        mutex_unlock(&inval_insert_mutex);

        wait_event_interruptible_timeout(the_queue, (sync_var & MASK_INVALIDATE) == 0, msecs_to_jiffies(100));

        n++;

        goto retry_mutex_inval_insert;
    }

    /* Comunico la presenza del thread che effettuerà l'inserimento di un nuovo blocco */
    __sync_fetch_and_add(&sync_var,1);

#ifdef NOT_CRITICAL_BUT_PUT
    printk("%s: [PUT DATA - SORTED LIST] Segnalata la presenza per l'inserimento del blocco %lld\n", MOD_NAME, index);
#endif

    mutex_unlock(&inval_insert_mutex);

    /* Recupero il nuovo blocco che dovrò inserire all'interno della lista */
    bh_x = get_block(index);

    if(bh_x == NULL)
    {
         __sync_fetch_and_sub(&sync_var,1);
        wake_up_interruptible(&the_queue);
        return 1;    
    }

    b_data_x = (struct soafs_block *)bh_x->b_data;

    if(b_data_x == NULL)
    {
        printk("%s: Errore con NULL\n", MOD_NAME);
        return 1;
    }

    /* Inizializzo il nuovo blocco del dispositivo da inserire nella Sorted List */

    /* Poiché il nuovo blocco viene inserito in fondo alla lista, il suo successore deve essere sbi->num_block (NULL) */
    b_data_x->next = sbi->num_block;

    /* Copio il messaggio utente nel blocco */
    bytes_ret = copy_from_user(b_data_x->msg, source, msg_size);

    /* Setto la dimensione del nuovo messaggio utente da inserire nel blocco */
    b_data_x->dim = msg_size - bytes_ret;

    /* Recupero il puntatore al superblocco del dispositivo contenente l'indice del blocco in testa alla Sorted List */
    bh_sb = get_sb_block();

    if(bh_sb == NULL)
    {
        if(bh_x != NULL)
            brelse(bh_x);
         
        __sync_fetch_and_sub(&sync_var,1);
        wake_up_interruptible(&the_queue);
        return 1;    
    }

    b_data_sb = (struct soafs_super_block *)bh_sb->b_data;

    if(b_data_sb == NULL)
    {
        printk("%s: Errore NULL pt2\n", MOD_NAME);
        return 1;
    }

    /*
     * Durante l'inserimento di un nuovo blocco, non è posibile avere in esecuzione alcuna invalidazione.
     * Tuttavia, è possibile avere in parallelo molteplici inserimenti. Di conseguenza, la dimensione della
     * Sorted List può solo che aumentare durante gli inserimenti. Come primo passo, verifico se la lista è
     * attualmente vuota in modo da eseguire un inserimento in testa.
     */

    if(b_data_sb->head_sorted_list == sbi->num_block)
    {
        ret_cmp = __sync_bool_compare_and_swap(&(b_data_sb->head_sorted_list), sbi->num_block, index);

        if(!ret_cmp)
            goto no_empty;
        
        if(bh_x != NULL)
            brelse(bh_x);
        
        if(bh_sb != NULL)
            brelse(bh_sb);

         __sync_fetch_and_sub(&sync_var,1);
        wake_up_interruptible(&the_queue);
        return 0;
    }

    /* 
     * Da questo momento, durante l'inserimento del nuovo blocco la sorted list non può diventare vuota
     * poiché non è possibile avere in concorrenza delle invalidazioni.
     */

no_empty:

    /* Recupero l'indice del blocco in testa alla Sorted List che è necessariamente differente da sbi->num_block */
    next_block_index = b_data_sb->head_sorted_list;

    /* Recupero il puntatore al blocco del dispositivo che rappresenta la testa della Sorted List */
    bh_b = get_block(next_block_index);

    if(bh_b == NULL)
    {
        if(bh_x != NULL)
            brelse(bh_x);
        
        if(bh_sb != NULL)
            brelse(bh_sb);

         __sync_fetch_and_sub(&sync_var,1);
        wake_up_interruptible(&the_queue);
        return 1;
    }

    b_data = (struct soafs_block *)bh_b -> b_data;

    if(b_data == NULL)
    {
        printk("%s: NULL pt3\n", MOD_NAME);
        return 1;
    }

    n = 0;

    count = 0;

retry_put_data_while:

    if(n > 20)
    {
        if(bh_x != NULL)
            brelse(bh_x);
        
        if(bh_sb != NULL)
            brelse(bh_sb);

        if(bh_b != NULL)
            brelse(bh_b);

         __sync_fetch_and_sub(&sync_var,1);
        wake_up_interruptible(&the_queue);
        return 1;
    }

    /* Ricerco l'ultimo blocco all'interno della Sorted List per eseguire un inserimento in coda */

    while(b_data->next != sbi->num_block)
    {

        count++;

        next_block_index = b_data->next;

        if(bh_b != NULL)
            brelse(bh_b);

        bh_b = get_block(next_block_index);

        if(bh_b == NULL)
        {
            if(bh_x != NULL)
                brelse(bh_x);
        
            if(bh_sb != NULL)
                brelse(bh_sb);

            __sync_fetch_and_sub(&sync_var,1);
            wake_up_interruptible(&the_queue);
            return 1;
        }

        b_data = (struct soafs_block *)bh_b -> b_data;
    }

    /* Gestisco la concorrenza con eventuali altri inserimenti in coda nella Sorted List */

    ret_cmp = __sync_bool_compare_and_swap(&(b_data->next), sbi->num_block, index);

    if(!ret_cmp)
    {
        n++;
        goto retry_put_data_while;
    }

    if(bh_x!=NULL)
        mark_buffer_dirty(bh_x);

    if(bh_sb!=NULL)
        mark_buffer_dirty(bh_sb);

    if(bh_b!=NULL)
        mark_buffer_dirty(bh_b);

#ifdef SYNC
    if(bh_x!=NULL)
        sync_dirty_buffer(bh_x);

    if(bh_sb!=NULL)
        sync_dirty_buffer(bh_sb);

    if(bh_b!=NULL)
        sync_dirty_buffer(bh_b);
#endif

    if(bh_x!=NULL)
        brelse(bh_x);

    if(bh_sb!=NULL)
        brelse(bh_sb);

    if(bh_b!=NULL)
        brelse(bh_b);

    //scan_sorted_list();

    __sync_fetch_and_sub(&sync_var,1);

    wake_up_interruptible(&the_queue);

    return 0;
}




/**
 * remove_data_block - Elimina effettivamente il blocco dalla Sorted List
 *
 * @index: Indice del blocco da eliminare dalla Sorted List
 *
 * @returns: Restituisce il puntatore ad una struttura dati che contiene il codice numerico
 *           che descrive l'esito della funzione e il puntatore ad una struttura dati buffer_head.
 */
static struct result_inval * remove_data_block(uint64_t index)
{
    int ret;

    uint64_t prev_index;
    uint64_t curr_index;
    uint64_t next_index;

    struct buffer_head *bh_sb;
    struct buffer_head *bh_block;
    struct buffer_head *bh_block_prev;
    struct soafs_super_block *b_data_sb;
    struct soafs_block *b_data_block;
    struct soafs_block *b_data_block_prev;

    struct result_inval *res_inval;
    struct soafs_sb_info *sbi;

    /* Alloco memoria per la struttura dati da restituire */
    res_inval = (struct result_inval *)kzalloc(sizeof(struct result_inval), GFP_KERNEL);

    if(res_inval == NULL)
    {
        printk("%s: [ERRORE INVALIDATE DATA] Errore nell'allocazione della struttura dati result_inval\n", MOD_NAME);
        return NULL;
    }

    //scan_sorted_list();

    /* Prendo il riferimento al superblocco che mantiene l'indice del blocco in testa alla Sorted List */
    bh_sb = get_sb_block();

    if(bh_sb == NULL)
    {
        printk("%s: [ERRORE INVALIDATE DATA] Errore nella lettura del superblocco\n", MOD_NAME);
        res_inval->code = 2;
        res_inval->bh = NULL;
        return res_inval;
    }

    b_data_sb = (struct soafs_super_block *)bh_sb->b_data;

    /* Prendo il riferimento al blocco in testa alla Sorted List */

    bh_block = get_block(b_data_sb->head_sorted_list);

    if(bh_block == NULL)
    {
        brelse(bh_sb);
        printk("%s: [ERRORE INVALIDATE DATA] Errore nella lettura del blocco in testa alla Sorted List\n", MOD_NAME);
        res_inval->code = 2;
        res_inval->bh = NULL;
        return res_inval;
    }

    b_data_block = (struct soafs_block *)bh_block->b_data;
        
    /*
     * A questo punto, osservo che il campo 'head_sorted_list' non può essere modificato
     * da nessun altro thread durante questo processo di invalidazione. Di conseguenza,
     * non ho alcun problema nell'eseguire la sb_bread poiché non potrà diventare sbi->num_block.
     */

    if(b_data_sb->head_sorted_list == index)
    {
        /* Eseguo una rimozione in testa */

        ret = __sync_bool_compare_and_swap(&(b_data_sb->head_sorted_list), index, b_data_block->next);

        if(!ret)
        {
            brelse(bh_sb);
            brelse(bh_block);
            res_inval->code = 3;
            res_inval->bh = NULL;
            return res_inval;
        }

        mark_buffer_dirty(bh_sb);

        brelse(bh_sb);

        res_inval->code = 0;

        res_inval->bh = bh_block;

        return res_inval;
    }

    /* L'elemento da invalidare non si trova in testa alla Sorted List e, quindi, lo devo ricercare all'interno della lista */
    
    /* Sicuramente il predecessore sarà almeno la testa della lista */ 
    prev_index = b_data_sb->head_sorted_list;

    /* Inizio ad iterare partendo dal secondo elemento all'interno della Sorted List */
    curr_index = b_data_block->next;

    /* Recupero le informazioni FS specific */
    sbi = (struct soafs_sb_info *)sb_global->s_fs_info;

    /*
     * Itero finché non trovo l'elemento all'interno della Sorted List oppure finché non raggiungo la fine della lista.
     * La fine della lista è rappresentata da sbi->num_block come valore del campo 'next'.   
     */

    while(curr_index != sbi->num_block)
    {

        brelse(bh_block);
    
        /* Prendo il riferimento al blocco del dispositivo su cui correntemente sto iterando */
        bh_block = get_block(curr_index);

        if(bh_block == NULL)
        {
            brelse(bh_sb);
            res_inval->code = 2;
            res_inval->bh = NULL;
            return res_inval;
        }
    
        b_data_block = (struct soafs_block *)bh_block->b_data;

        if(curr_index == index)
        {
            /* Recupero l'indice del blocco successivo al blocco che devo invalidare all'interno della Sorted List */
            next_index = b_data_block->next;
            break;
        }

        prev_index = curr_index;

        curr_index = b_data_block->next;
    }
    
    /*
     * Verifico se il blocco richiesto non è stato trovato e, quindi, sono arrivato alla fine della Sorted List.
     * Il blocco da invalidare deve essere necessariamente presente all'interno della Sorted List.    
     */
    if(curr_index == sbi->num_block)
    {
        printk("%s: Errore la lista è stata attraversata totalmente\n", MOD_NAME);
        brelse(bh_sb);
        brelse(bh_block);
        res_inval->code = 3;
        res_inval->bh = NULL;
        return res_inval;
    }

    /*
     * Prendo il riferimento al blocco che precede il blocco da rimuovere dalla Sorted List. Il successore
     * del predecessore del blocco da rimuovere diventa il successore del blocco da rimuovere. L'indice del
     * blocco successore è mantenuto all'interno della variabile 'next_index'
     */
    bh_block_prev = get_block(prev_index);

    if(bh_block_prev == NULL)
    {
        printk("%s: [ERRORE INVALIDATE DATA] Errore nella lettura del blocco predecessore con indice %lld\n", MOD_NAME, prev_index);
        brelse(bh_sb);
        brelse(bh_block);
        res_inval->code = 2;
        res_inval->bh = NULL;
        return res_inval;
    }
    
    b_data_block_prev = (struct soafs_block *)bh_block_prev->b_data;
    
    /* Eseguo la rimozione dell'elemento all'interno della Sorted List scollegandolo dalla lista ordinata */
    ret = __sync_bool_compare_and_swap(&(b_data_block_prev->next), index, next_index);

    if(!ret)
    {
        printk("%s: Errore nella compare and swap per la modifica del predecessore\n", MOD_NAME);
        printk("%s: Indice richiesto da invalidare %lld\tIndice successore del predecessore %lld\n", MOD_NAME, index, b_data_block_prev->next);
        brelse(bh_sb);
        brelse(bh_block);
        brelse(bh_block_prev);
        res_inval->code = 3;
        res_inval->bh = NULL;
        return res_inval;
     }

    mark_buffer_dirty(bh_block_prev);

    brelse(bh_block_prev);
    
    brelse(bh_sb);

    res_inval->code = 0;

    res_inval->bh = bh_block;

    return res_inval;
}




/**
 * invalidate_data_block - Invalida un blocco valido contenente un messaggio utente
 *
 * @index: Indice del blocco da invalidare
 *
 * Durante un'invalidazione non è possibile che la stato della Sorted List venga modificato.
 * Infatti, durante un''invalidazione non è possibile avere un'ulteriore invalidazione oppure
 * l'inserimento di un nuovo blocco. Una volta avvisati gli altri thread sull'invalidazione
 * che deve essere eseguita, il thread ricerca il blocco target all'interno della Sorted List
 * per rimuoverlo e per invalidarlo.
 *
 * Restituisce il valore 0 in caso di successo; altrimenti può restituire i seguenti valori:
 * - 1 se il blocco già era non valido e il thread non l'ha invalidato
 * - 2 se si è verificato un errore durante l'invalidazione
 * - 3 se si è verificato un comportamento anomalo nell'invalidazione
 */
int invalidate_data_block(uint64_t index)
{
    int n;
    int index_sorted;
    uint64_t num_insert;

    unsigned long last_epoch_sorted;
    unsigned long updated_epoch_sorted;
    unsigned long grace_period_threads_sorted;

    struct block_free *bf;
    struct soafs_sb_info *sbi;
    struct result_inval *res_inval;

    /* 
     * Prendo il mutex per eseguire il processo di invalidazione. Non è possibile
     * avere 2+ invalidazioni contemporaneamente. Le invalidazioni sono eseguite in
     * sequenza, una dopo l'altra.
     */
    mutex_lock(&invalidate_mutex);

    /*
     * Prima di effettuare l'invalidazione, è necessario verificare se sono in
     * esecuzione degli inserimenti. Nel caso in cui ci siano degli inserimenti
     * in corso (almeno uno), l'invalidazione dovrà essere posticipata; altrimenti,
     * si comunica l'inzio della invalidazione e si procede con la rimozione del blocco. 
     */

    n = 0;

retry_invalidate:

    if(n > 20)
    {
        printk("%s: [ERRORE INVALIDATE DATA] Il numero massimo di tentativi per l'invalidazione del blocco %lld è stato raggiunto\n", MOD_NAME, index);
        mutex_unlock(&invalidate_mutex);
        return 2;
    }

    /* Sezione critica tra inserimenti e invalidazioni */
    mutex_lock(&inval_insert_mutex);

    /* Recupero il numero di inserimenti in corso */
    num_insert = sync_var & MASK_NUMINSERT;

#ifdef NOT_CRITICAL_INVAL
    printk("%s: [INVALIDATE DATA] Il numero di inserimenti attualmente in corso è pari a %lld\n", MOD_NAME, num_insert);
#endif

    if(num_insert > 0)
    {
        mutex_unlock(&inval_insert_mutex);

#ifdef NOT_CRITICAL_BUT_INVAL
        printk("%s: [ERRORE INVALIDATE DATA] L'invalidazione del blocco %lld non è stata effettuata al tentativo #%d\n", MOD_NAME, index, n);
#endif

        wait_event_interruptible_timeout(the_queue, (sync_var & MASK_NUMINSERT) == 0, msecs_to_jiffies(100));

        n++;

#ifdef NOT_CRITICAL_INVAL
        printk("%s: [ERRORE INVALIDATE DATA] Tentativo #%d per l'invalidazione del blocco %lld\n", MOD_NAME, n, index);
#endif

        goto retry_invalidate;
    }

    if(sync_var)
    {
        printk("%s: [ANOMALIA INVALIDATE DATA] Verificare il valore di sync_var\n", MOD_NAME);
        mutex_unlock(&inval_insert_mutex);
        mutex_unlock(&invalidate_mutex);
        return 3;
    }

    /* Comunico l'inizio del processo di invalidazione */
    __atomic_exchange_n (&(sync_var), 0X8000000000000000, __ATOMIC_SEQ_CST);

    /* Termino la sezione critica */
    mutex_unlock(&inval_insert_mutex);

    /* 
     * Arrivato a questo punto, sono l'unico thread in esecuzione che può effettivamente modificare
     * lo stato del dispositivo. Poiché sono arrivato fino a questo punto, il controllo sul bit di
     * validità eseguito nella system call precedentemente è terminato con successo. Di conseguenza,
     * il blocco richiesto poteva effettivamente essere invalidato. Tuttavia, potrei essere stato in
     * attesa per entrare in sezione critica e, nel frattempo, qualche altro thread potrebbe aver
     * invalidato il blocco richiesto. A questo punto, eseguo un altro controllo sullo stato di
     * validità del blocco. Poiché no c'é nessuna altra invalidazione in corso, se questo blocco è
     * ancora valido allora posso procedere con l'invalidazione effettiva.
     */

    if(!check_bit(index))
    {
        __atomic_exchange_n (&(sync_var), 0X0000000000000000, __ATOMIC_SEQ_CST);
        mutex_unlock(&invalidate_mutex);
        wake_up_interruptible(&the_queue);
        return 1;
    }

    /* Rimuove effettivamente il blocco dalla Sorted List nel device */
    res_inval = remove_data_block(index);

    //TODO: Vedi se la gestione dell'errore è corretta
    if( (res_inval == NULL) || res_inval->code )
    {
        __atomic_exchange_n (&(sync_var), 0X0000000000000000, __ATOMIC_SEQ_CST);
        mutex_unlock(&invalidate_mutex);
        wake_up_interruptible(&the_queue);
        return res_inval->code;
    }

    updated_epoch_sorted = (gp->next_epoch_index_sorted) ? MASK : 0;

    gp->next_epoch_index_sorted += 1;
    gp->next_epoch_index_sorted %= 2;

    last_epoch_sorted = __atomic_exchange_n (&(gp->epoch_sorted), updated_epoch_sorted, __ATOMIC_SEQ_CST);

    index_sorted = (last_epoch_sorted & MASK) ? 1:0;

    grace_period_threads_sorted = last_epoch_sorted & (~MASK);

#ifdef NOT_CRITICAL_INVAL
    printk("%s: [INVALIDATE DATA] Attesa della terminazione del grace period lista ordinata: #threads %ld\n", MOD_NAME, grace_period_threads_sorted);
#endif

sleep_again:

    wait_event_interruptible_timeout(the_queue, gp->standing_sorted[index_sorted] >= grace_period_threads_sorted, msecs_to_jiffies(100));    

#ifdef NOT_CRITICAL_INVAL
    printk("%s: [INVALIDATE DATA] gp->standing_sorted[index_sorted] = %ld\tgrace_period_threads_sorted = %ld\n", MOD_NAME, gp->standing_sorted[index_sorted], grace_period_threads_sorted);
#endif

    if(gp->standing_sorted[index_sorted] < grace_period_threads_sorted)
    {
        printk("%s: [ERRORE INVALIDATE DATA] Il thread invalidate va nuovamente a dormire per l'invalidazione del blocco %lld\n", MOD_NAME, index);
        goto sleep_again;
    }

    gp->standing_sorted[index_sorted] = 0;

    /* Recupero le informazioni FS specific */
    sbi = (struct soafs_sb_info *)sb_global->s_fs_info;

    // TODO: Terminato il Grace Period posso aggiornare il puntatore del blocco in modo sicuro
    ((struct soafs_block *)res_inval->bh->b_data)->next = sbi->num_block;
    asm volatile ("mfence");

retry_kmalloc_invalidate_block:

    /*
     * Una volta che il blocco è stato invalidato con successo, posso inserire il suo indice
     * all'interno della lista dei blocchi liberi. Da questo momento, il blocco può essere
     * utilizzato per l'inserimento di nuovi messaggi utente.
     */
    bf = (struct block_free *)kmalloc(sizeof(struct block_free), GFP_KERNEL);

    if(bf == NULL)
    {
        printk("%s: [ERRORE INVALIDATE DATA] Errore esecuzione kmalloc() a seguito della rimozione\n", MOD_NAME);
        goto retry_kmalloc_invalidate_block;
    }
    
    bf->block_index = index;

    bf->next = NULL;

    insert_free_list_conc(bf);

    /*
     * A seguito della rimozione del blocco dalla Sorted List e dell'inserimento del suo indice
     * all'interno della lista dei blocchi liberi, setto il relativo bit di validità a 0.
     */

    set_bitmask(index,0);

    __atomic_exchange_n (&(sync_var), 0X0000000000000000, __ATOMIC_SEQ_CST);

    /* Rilascio il mutex per permettere successive invalidazioni */
    mutex_unlock(&invalidate_mutex);

    wake_up_interruptible(&the_queue);    

    return res_inval->code;
}




/*
 * insert_free_list_conc - Inserisce un nuovo elemento in testa alla Free List
 *
 * @item: Elemento da inserire in testa alla lista
 *
 * @returns: La funzione non restituisce alcun valore
 */
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




/**
 * check_consistenza - Controllo sull'inizializzazione della lista dei blocchi liberi
 * 
 * Questa funzione implementa un semplice controllo sulla consistenza delle informazioni
 * presenti nella lista dei blocchi liberi rispetto alle informazioni mantenute nella
 * bitmask. Gli indici dei blocchi inseriti all'interno della lista devono avere il
 * corrispondente bit della bitmask pari a 0.
 *
 * @returns: La funzione non restituisce alcun valore
 */
static void check_consistenza(void)
{
    int ret;
    struct block_free *bf;

    /* Recupero il riferimento alla testa della lista dei blocchi liberi */
    bf = head_free_block_list;

    /* Eseguo la scansione della lista dei blocchi liberi */
    while(bf != NULL)
    {
        printk("%s: [CHECK CONSISTENZA FREE-LIST] Valore indice del blocco: %lld\n", MOD_NAME, bf->block_index);
    
        ret = check_bit(bf->block_index);

        if(ret)
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
 * get_freelist_head - Recupero l'elemento in testa alla Free List
 *
 * Restituisce l'indice del blocco libero che si trova in testa alla lista.
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




/**
 * check_bit - Verifica la validità di un blocco di dati
 *
 * @index: Indice del blocco di cui si vuole verificare la validità
 * 
 * @returns: La funzione restituisce il valore 1 se il blocco di dati è valido,
 * il valore 0 se il blocco di dati non è valido e il valore 2 se si è verificato un errore.
 */
int check_bit(uint64_t index)
{
    int bitmask_entry;
    int array_entry;
    int bits;
    uint64_t base;
    uint64_t offset;
    uint64_t *block_state;
    struct buffer_head *bh;

    bits = sizeof(uint64_t) * 8;

    base = 1;

    bitmask_entry = index / (SOAFS_BLOCK_SIZE << 3);

    bh = sb_bread(sb_global, 2 + bitmask_entry);

    if(bh == NULL){
        printk("%s: [ERRORE CHECK BIT] Errore nella lettura del blocco di stato\n", MOD_NAME);
        return 2;
    }

    block_state = (uint64_t *)bh->b_data;

    array_entry = (index  % (SOAFS_BLOCK_SIZE << 3)) / bits;

    offset = index % bits;

    if(block_state[array_entry] & (base << offset))
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




/**
 * get_bitmask_block - Recupera gli indici dei blocchi liberi e li inserisce nella free_block_list
 *
 * Per eseguire la ricerca degli indici di nuovi blocchi liberi, si utilizza la struttura dati
 * della bitmask che è presente all'interno dei blocchi di stato del dispositivo. Per ridurre
 * il costo della ricerca degli indici, si sfrutta la variabile 'pos' che indica la posizione
 * all'interno della bitmask a partire dalla quale bisogna iniziare la ricerca dei blocchi
 * liberi. Infatti, tutti i blocchi il cui indice si trova prima della variabile 'pos' sono
 * occupati.
 *
 * @returns: La funzione restituisce il valore 0 se ha completato con successo; altrimenti,
 * restituisce il valore 1.
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
     * Devo gestire la concorrenza tra i vari scrittori. Può accadere che più thread nello
     * stesso momento devono invocare questa funzione. Solamente un thread alla volta può
     * eseguirla in modo da caricare nella lista le informazioni. Dopo che il primo thread
     * ha eseguito questa funzione, i threads successivi possono trovare la lista non vuota,
     * poiché precedentemente popolata, oppure la lista nuovamente vuota ma con tutti i blocchi
     * liberi già utilizzati. In entrambi i casi, la funzione non deve essere eseguita poiché
     * o i blocchi sono stati già recuperati oppure non esiste alcun blocco libero da utilizzare
     * per inserire il messaggio.
     */

    if( (head_free_block_list != NULL) || (num_block_free_used == sbi->num_block_free))
    {

#ifdef NOT_CRITICAL_BUT_PUT
        printk("%s: [PUT DATA - RECUPERO BLOCCHI] I blocchi liberi sono stati già determinati o quelli occupati invalidati\n", MOD_NAME);
#endif

        return 0;
    }

    /* Recupero il numero totale dei blocchi di dati */
    num_block_data = sbi->num_block - 2 - sbi->num_block_state;

    /* 
     * Questa variabile rappresenta il numero massimo di blocchi che è possibile inserire
     * all'interno della lista. In questo modo, riesco a gestire meglio la quantità di
     * memoria che viene utilizzata. 
     */
    count = sbi->update_list_size;

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

retry_get_bitmask_block:

            /* Implemento un inserimento in testa alla lista free_block_list */
            old_head = head_free_block_list;
        
            bf->next = head_free_block_list;

            ret = __sync_bool_compare_and_swap(&head_free_block_list, old_head, bf);

            if(!ret)
            {

#ifdef NOT_CRITICAL_BUT_PUT
                    printk("%s: [ERRORE PUT DATA - RECUPERO BLOCCHI] Errore inserimento del blocco in concorrenza\n", MOD_NAME);
#endif

                    goto retry_get_bitmask_block;
            }

            pos = index + 1;

#ifdef NOT_CRITICAL_PUT
            printk("%s: [PUT DATA - RECUPERO BLOCCHI] pos = %lld\n", MOD_NAME, pos);
#endif

            num_block_free_used++;

            asm volatile ("mfence");

            count --;

            /*
             * Verifico se ho esaurito il numero di blocchi liberi sul dispositivo oppure se ho
             * già caricato il numero massimo di blocchi consentito all'interno della lista.
             * In entrambi i casi, interrompo la ricerca in modo da risparmiare le risorse.
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




/**
 * insert_free_list - Inserisce un nuovo elemento all'interno della lista free_block_list
 *
 * @index: Indice del blocco libero da inserire nella lista
 *
 * L'inserimento nella lista viene fatto in testa poiché non mi importa mantenere
 * alcun ordine tra i blocchi liberi del dispositivo.
 *
 * Restituisce il valore 0 in caso di successo; altrimenti, restituisce il valore 1.
 */
int insert_free_list(uint64_t index)
{
    struct block_free *new_item;
    struct block_free *old_head;

    /* Alloco il nuovo elemento da inserire nella lista */
    new_item = (struct block_free *)kmalloc(sizeof(struct block_free), GFP_KERNEL);

    if(new_item == NULL)
    {
        printk("%s: [ERRORE INIZIALIZZAZIONE CORE - FREE LIST] Errore esecuzione kmalloc() per il blocco %lld\n", MOD_NAME, index);
        return 1;
    }

    /* Popolo la struttura dati che rappresenta il nuovo elemento della lista */
    new_item->block_index = index;

    /* Recupero il riferimento alla testa della lista corrente */
    old_head = head_free_block_list;

    /* Aggiorno la testa della lista */
    head_free_block_list = new_item;

    /* Collego la nuova testa della lista con quella precedente */
    head_free_block_list -> next = old_head;

#ifdef NOT_CRITICAL_INIT
    printk("%s: [INIZIALIZZAZIONE CORE - FREE LIST] Il blocco %lld è stato inserito nella lista dei blocchi liberi\n", MOD_NAME, index);
#endif

    return 0;
}




/**
 * init_free_block_list - Inizializzazione della free_block_list
 * 
 * @index_free: L'array contenente gli indici dei blocchi liberi che si vogliono
 *              caricare nella lista
 *
 * @actual_size: La dimensione effettiva dell'array 'index_free'
 *
 * Questa funzione ha il compito di inizializzare la lista contenente gli indici
 * dei blocchi liberi. Viene utilizzato direttamente l'array 'index_free' in modo
 * da evitare ulteriori accessi al dispositivo. Il caricamento di un insieme
 * iniziale di indici dei blocchi liberi viene fatto in modo da rendere più rapida
 * l'identificazione di un nuovo blocco libero da utilizzare per la scrittura.
 * La variabile 'num_block_free_used' rappresenta il numero di blocchi liberi
 * all'istante di montaggio che sono stati caricati all'interno della lista. A
 * seguito dell'inizializzazione, essa viene settata alla dimensione dell'array
 * utilizzato per l'inizializzazione poiché sono stati caricati esattamente quegli
 * indici.
 * La variabile 'pos' rappresenta la posizione all'interno della bitmask a partire
 * dalla quale verranno cercati i blocchi liberi nel dispositivo. Questa variabile
 * consente di ridurre il costo della ricerca di un nuovo blocco libero poiché evita
 * la scansione di una porzione iniziale della bitmask che risulta essere già occupata.
 *
 * Restituisce il valore 0 in caso di successo; altrimenti, restituisce il valore 1.
 */
int init_free_block_list(uint64_t *index_free, uint64_t actual_size)
{
    uint64_t index;
    uint64_t roll_index;
    int ret;
    struct block_free *roll_bf;

    /* Il valore SIZE_INIT rappresenta la dimensione massima dell'array index_free.
     * Di conseguenza, la dimensione corrente 'actual_size' dell'array index_free 
     * non può essere maggiore della dimensione massima consentita SIZE_INIT.
     */

    if(SIZE_INIT < actual_size)
    {
        printk("%s: [ERRORE INIZIALIZZAZIONE CORE - FREE LIST] ACTUAL_SIZE = %lld\tSIZE_INIT = %d\n", MOD_NAME, actual_size, SIZE_INIT);
        return 1;
    }

    /* Eseguo la scansione dell'array per recuperare gli indici dei blocchi */
    for(index=0; index<actual_size;index++)
    {

#ifdef NOT_CRITICAL_INIT
        printk("%s: [INIZIALIZZAZIONE CORE - FREE LIST] Inserimento del blocco %lld all'interno della lista...\n", MOD_NAME, index_free[index]);
#endif

        ret = insert_free_list(index_free[index]);

        if(ret)
        {
            printk("%s: [ERRORE INIZIALIZZAZIONE CORE - FREE LIST] Errore inserimento del blocco con indice %lld\n", MOD_NAME, index);

            /*
             * Eseguo la procedura di rollback per deallocare gli elementi
             * che sono stati allocati fino alla iterazione corrente. La
             * variabile 'index' mi dice fino a che punto sono arrivato
             * nelle iterazioni.
             */

            for(roll_index=0; roll_index<index;roll_index++)
            {
                roll_bf = head_free_block_list->next;
                kfree(head_free_block_list);
                head_free_block_list = roll_bf;
            }

            if(head_free_block_list != NULL)
            {
                printk("%s: [ERRORE INIZIALIZZAZIONE CORE - FREE LIST] Errore rollback nella deallocazione della lista dei blocchi liberi\n", MOD_NAME);
            }
            else
            {
                printk("%s: [ERRORE INIZIALIZZAZIONE CORE - FREE LIST] Rollback eseguito con successo...\n", MOD_NAME);
            }

            return 1;
        }

#ifdef NOT_CRITICAL_INIT
        printk("%s: [INIZIALIZZAZIONE CORE - FREE LIST] Blocco #%lld inserito correttamente all'interno della lista\n", MOD_NAME, index_free[index]); 
#endif    
   
    }

    num_block_free_used = actual_size;

    pos = index_free[actual_size - 1] + 1;

    printk("%s: [INIZIALIZZAZIONE CORE - FREE LIST] num_block_free_used = %lld\n", MOD_NAME, num_block_free_used);

    printk("%s: [INIZIALIZZAZIONE CORE - FREE LIST] pos = %lld\n", MOD_NAME, pos);

    check_consistenza();

    return 0;
}




/**
 * set_bitmask - Modifica le informazioni di validità dei blocchi all'interno della bitmask
 *
 * @index: Indice del blocco di cui si vuole cambiare la stato di validità
 * @mode: il valore 0 consente di settare il blocco come non valido mentre il valore 1
 *        setta il blocco come valido.
 *
 * Restituisce il valore 0 in caso di successo; altrimenti, restituisce il valore 1.
 */
int set_bitmask(uint64_t index, int mode)
{
    int bits;
    int array_entry;
    int bitmask_entry;
    uint64_t base;
    uint64_t shift_base;
    uint64_t offset;
    uint64_t *block_state;
    struct buffer_head *bh;


    bits = sizeof(uint64_t) * 8;    

    /* Determino il blocco di stato che contiene l'informazione relativa al blocco richiesto */
    bitmask_entry = index / (SOAFS_BLOCK_SIZE << 3);

    bh = sb_bread(sb_global, 2 + bitmask_entry);
    
    if(bh == NULL)
    {
        printk("%s: [ERRORE SET BITMASK] Errore nella lettura del blocco di stato dal dispositivo\n", MOD_NAME);
        return 1;
    }

    block_state = (uint64_t *)bh->b_data;    

    /* Determino la entry dell'array */
    array_entry = (index  % (SOAFS_BLOCK_SIZE << 3)) / bits;

    /* Determino l'offset nella entry dell'array */
    offset = index % bits;
    
    base = 1;

    shift_base = base << offset;

    if(mode)
    {
        __sync_fetch_and_or(&(block_state[array_entry]), shift_base);
    }
    else
    {
        __sync_fetch_and_xor(&(block_state[array_entry]), shift_base);
    }

    return 0;  

}




/**
 * init_data_structure_core - Inizializzazione delle strutture dati core del modulo
 *
 * @num_data_block: Numero dei blocchi di dati del dispositivo
 * @index_free: array contenente gli indici dei blocchi liberi all'istante di montaggio
 * @actual_size: Numero degli indici dei blocchi liberi nell'array all'istante di montaggio
 *
 * La struttura dati che viene inizializzata è la lista dei blocchi liberi Free List.
 *
 * @returns: La funzinoe restituisce il valore 0 se l'inizializzazione è andata a buon fine;
 *           altrimenti viene restituito il valore 1.
 */
int init_data_structure_core(uint64_t num_data_block, uint64_t *index_free, uint64_t actual_size)
{
    int i;
    int ret;
    struct soafs_sb_info *sbi;

    /* E' necessario avere il riferimento al superblocco per poter eseguire le operazioni di inizializzazione */
    if(sb_global == NULL)
    {
        printk("%s: [ERRORE INIZIALIZZAZIONE CORE] Il superblocco globale non è valido\n", MOD_NAME);
        return 1;
    }

    /* Il numero dei blocchi di dati del dispositivo deve essere > 0 */
    if(num_data_block <= 0)
    {
        printk("%s: [ERRORE INIZIALIZZAZIONE CORE] Il numero dei blocchi di dati del device non è valido\n", MOD_NAME);
        return 1;
    }

    /* Recupero le informazioni che sono FS specific */
    sbi = (struct soafs_sb_info *)sb_global->s_fs_info;

    printk("%s: [INIZIALIZZAZIONE CORE] Valore di 'actual_size' è pari a %lld\n", MOD_NAME, actual_size);

    /*
     * Effettuo i seguenti controlli di consistenza:
     * 1. La dimensione dell'array degli indici liberi 'index_free' non può essere negativa.
     * 2. Il numero dei blocchi liberi nel dispositivo all'istante di montaggio non può essere negativo.
     * 3. Non è possibile avere degli indici dei blocchi liberi se il numero dei blocchi liberi è zero.
     */
    if( (actual_size < 0) || (sbi->num_block_free < 0) || ((actual_size > 0) && (sbi->num_block_free == 0)) )
    {
        printk("%s: [ERRORE INIZIALIZZAZIONE CORE] Le informazioni relative ai blocchi liberi non sono valide\n", MOD_NAME);
        return 1;
    }

    /*
     * Se la dimensione effettiva dell'array 'index_free' è strettamente
     * maggiore di zero allora esistono degli indici di blocchi liberi
     * che devono essere memorizzati all'interno della lista dei blocchi
     * liberi. Se la dimensione effettiva dell'array è pari a zero, allora
     * nessun indice verrà caricato all'interno della struttura dati e la
     * lista sarà inizialmente vuota.
     */
    if(actual_size > 0)
    {
        /*
        * Viene inizializzata la struttura dati free_block_list utilizzando
        * le informazioni presenti all'interno del superblocco del dispositivo.
        * Vengono utilizzati l'array contenente gli indici dei blocchi liberi
        * all'istante di montaggio e la dimensione effettiva dell'array.
        */

        ret = init_free_block_list(index_free, actual_size);
        if(ret)
        {
            printk("%s: [ERRORE INIZIALIZZAZIONE CORE] L'inizializzazione della free_block_list non è terminata con successo\n", MOD_NAME);
            return 1;
        }

    }
    else
    {
        /* Non ho utilizzato alcun blocco libero all'istante di montaggio */
        num_block_free_used = 0;

        /* Si inizia la ricerca dei blocchi liberi dall'indice zero della bitmask */
        pos = 0;

        /* La lista dei blocchi liberi è vuota */
        head_free_block_list = NULL;
    }

    /* Inizializzazione della struttura dati per la gestione del Grace Period */

    gp = (struct grace_period *)kmalloc(sizeof(struct grace_period), GFP_KERNEL);

    if(gp == NULL)
    {
        printk("%s: [ERRORE INIZIALIZZAZIONE CORE] Errore inizializzazione struttura dati per il grace period\n", MOD_NAME);
        free_all_memory();
        return 1;
    }

    gp->epoch_sorted = 0x0;

    gp->next_epoch_index_sorted = 0x1;

    for(i=0;i<EPOCHS;i++)
    {
        gp->standing_sorted[i] = 0x0;
    }

    debugging_init();

    return 0;
}
