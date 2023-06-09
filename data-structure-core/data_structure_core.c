#include <linux/buffer_head.h>  /* sb_bread()-brelse() */
#include <linux/string.h>       /* strncpy() */
#include <linux/slab.h>         /* kmalloc() */
#include <linux/wait.h>         /* wait_event_interruptible() - wake_up_interruptible() */
#include <linux/jiffies.h>

#include "../headers/main_header.h"



struct block *head_sorted_list = NULL;              /* Puntatore alla testa della lista contenente i blocchi nell'ordine di consegna */
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
 * La funzione non restituisce alcun valore.
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
 * scan_sorted_list - Esegue la scansione della struttura dati Sorted List
 *
 * Questa funzione ha prevalentemente uno scopo per il debugging del modulo
 * 
 * La funzione non restituisce alcun valore.
 */
static void scan_sorted_list(void)
{
    struct block *curr;

    curr = head_sorted_list;
    
    printk("%s: ---------------------------------- INIZIO SORTED LIST ---------------------------------------------", MOD_NAME);

    while(curr!=NULL)
    {
        printk("Blocco #%lld\n", curr->block_index);
        curr = curr->sorted_list_next;
    }

    printk("%s: ----------------------------------  FINE SORTED LIST  ---------------------------------------------", MOD_NAME);
    
}



/**
 * debugging_init - Esegue la scansione delle strutture dati core del modulo
 *
 * Questa funzione ha prevalentemente uno scopo per il debugging del modulo
 * 
 * La funzione non restituisce alcun valore.
 */
static void debugging_init(void)
{
    /* scansione della lista contenente le informazioni dei blocchi liberi. */
    scan_free_list();

    /* scansione della lista contenente i blocchi ordinati per inserimento. */
    scan_sorted_list();
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



/**
 * check_consistenza - Controllo sull'inizializzazione della lista dei blocchi liberi
 * 
 * Questa funzione implementa un semplice controllo sulla consistenza delle informazioni
 * presenti nella lista dei blocchi liberi rispetto alle informazioni mantenute nella
 * bitmask. Gli indici dei blocchi inseriti all'interno della lista devono avere il
 * corrispondente bit della bitmask pari a 0.
 *
 * La funzione non restituisce alcun valore
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



/**
 * check_bit - Verifica la validità di un blocco di dati
 *
 * @index: Indice del blocco di cui si vuole verificare la validità
 * 
 * Restituisce il valore 1 se il blocco di dati è valido, il valore 0 se il
 * blocco di dati non è valido e il valore 2 se si è verificato un errore.
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
 * Restituisce il valore 0 se ha completato con successo; altrimenti, restituisce il valore 1.
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
 * remove_block - Rimozione del blocco target dalla Sorted List
 *
 * @index: Indice del blocco che deve essere rimosso dalla lista
 *
 * Il blocco target viene cercato all'interno della lista per poterlo eliminare.
 *
 * La funzione restituisce il puntatore all'elemento che viene rimosso dalla lista
 */
static struct block *remove_block(uint64_t index)
{
    struct block *next;
    struct block *curr;
    struct block *prev;
    struct block *invalid_block;

    /* Verifico se la Sorted List è vuota */
    if(head_sorted_list == NULL)
    {
        printk("%s: [ERRORE INVALIDATE DATA - REMOVE] La lista risulta essere vuota durante il processo di invalidazione\n", MOD_NAME);
        return NULL;
    }

    /* Verifico se l'elemento da invalidare è la testa della lista */
    if( head_sorted_list->block_index == index )
    {
        /* Prendo il riferimento al blocco che dovrà essere deallocato */
        invalid_block = head_sorted_list;

        /* Eseguo una rimozione in testa dell'elemento target */
        head_sorted_list = head_sorted_list->sorted_list_next;

        asm volatile ("mfence");

#ifdef NOT_CRITICAL_INVAL
        printk("%s: [INVALIDATE DATA - REMOVE] Il blocco %lld richiesto per l'invalidazione è stato eliminato con successo dalla lista ordinata\n", MOD_NAME, index);
#endif

        return invalid_block;
    }

    /* Ricerco il blocco target da invalidare nella Sorted List */

    prev = head_sorted_list;

    curr = prev->sorted_list_next;

    while(curr!=NULL)
    {
        if(curr->block_index == index)
        {

#ifdef NOT_CRITICAL_INVAL
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
        printk("%s: [ERRORE INVALIDATE DATA - REMOVE] Il blocco %lld non è presente nella Sorted List\n", MOD_NAME, index);
        return NULL;
    }

    /* Il successore del predecessore è il successore dell'elemento da eliminare */
    prev->sorted_list_next = next;
 
    asm volatile ("mfence");

    return curr;
}



/**
 * invalidate_block - Invalida un blocco contenente un messaggio utente valido
 *
 * @index: Indice del blocco che deve essere invalidato
 *
 * Durante una invalidazione non è possibile che la stato della Sorted List venga modificato.
 * Infatti, durante una invalidazione non è possibile avere un'ulteriore invalidazione oppure
 * l'inserimento di un nuovo blocco. Una volta avvisati gli altri thread sull'invalidazione
 * che deve essere eseguita, il thread ricerca il blocco target all'interno della Sorted List
 * per rimuoverlo.
 *
 * La funzione restituisce il valore 0 in caso di successo; altrimenti, ritorna il valore 1.
 */
int invalidate_block(uint64_t index)
{
    int n;
    int index_sorted;
    uint64_t num_insert;
    unsigned long last_epoch_sorted;
    unsigned long updated_epoch_sorted;
    unsigned long grace_period_threads_sorted;
    struct block_free *bf;
    struct block *invalid_block;

    /*
     * Prima di effettuare l'invalidazione, è necessario verificare se sono in
     * esecuzione degli inserimenti. Nel caso in cui  ci siano degli inserimenti
     * in corso (almeno uno), l'invalidazione dovrà essere posticipata; altrimenti,
     * si comunica l'inzio della invalidazione e si procede con la rimozione del blocco. 
     */
    
    n = 0;

    /* 
     * Prendo il mutex per eseguire il processo di invalidazione. Non è possibile
     * avere 2+ invalidazioni contemporaneamente. Le invalidazioni sono eseguite in
     * sequenza, una dopo l'altra.
     */
    mutex_lock(&invalidate_mutex);

retry_invalidate:

    /* Verifico se sono terminati i tentativi disponibili per l'invalidazione */
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
        return 1;
    }

    /* Comunico l'inizio del processo di invalidazione */
    __atomic_exchange_n (&(sync_var), 0X8000000000000000, __ATOMIC_SEQ_CST);

    /* Termino la sezione critica */
    mutex_unlock(&inval_insert_mutex);

    invalid_block = remove_block(index);

    if(invalid_block == NULL)
    {
        __atomic_exchange_n (&(sync_var), 0X0000000000000000, __ATOMIC_SEQ_CST);
        mutex_unlock(&invalidate_mutex);
        wake_up_interruptible(&the_queue);
        return 1;
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
    printk("%s: gp->standing_ht[index_ht] = %ld\tgrace_period_threads_ht = %ld\tgp->standing_sorted[index_sorted] = %ld\tgrace_period_threads_sorted = %ld\n", MOD_NAME, gp->standing_ht[index_ht], grace_period_threads_ht, gp->standing_sorted[index_sorted], grace_period_threads_sorted);
#endif

    if(gp->standing_sorted[index_sorted] < grace_period_threads_sorted)
    {
        printk("%s: [ERRORE INVALIDATE DATA] Il thread invalidate va nuovamente a dormire per l'invalidazione del blocco %lld\n", MOD_NAME, index);
        goto sleep_again;
    }

    gp->standing_sorted[index_sorted] = 0;

    /* Terminato il Grace Period posso dealocare la struttura dati in modo sicuro */
    kfree(invalid_block);

retry_kmalloc_invalidate_block:

    /*
     * Una volta che il blocco è stato ivnalidato con successo, posso inserire il suo indice
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

    
    return 0;
}



/**
 * insert_sorted_list_conc - Inserimento di un nuovo elemento nella Sorted List
 *
 * @block: Il nuovo elemento da aggiungere all'interno della lista
 *
 * A differenza della funzione insert_sorted_list, questa funzione gestisce la concorrenza.
 * In questo scenario, l'inserimento dei blocchi all'interno della lista viene fatto in coda.
 * E' necessario gestire esplicitamente la concorrenza con le invalidazioni. Se viene eseguita
 * l'invalidazione dell'ultimo blocco nella Sorted List in parallelo con l'inserimento, allora
 * è possibile che si verifichi una rottura della lista.
 *
 * La funzione restituisce il valore 0 in caso di successo; altrimenti, restituisce il valore 1.
 */
int insert_sorted_list_conc(struct block *block)
{
    int n;
    int ret;
    struct block *next;
    struct block *curr;

    next = NULL;

    /* Poiché l'elemento viene inserito in coda alla lista, allora il suo succesore sarà NULL */
    block->sorted_list_next = NULL;   

    /* Gestione della concorrenza con le invalidazioni */
    n = 0;

retry_mutex_inval_insert:

    if(n > 10)
    {
        printk("%s: [ERRORE PUT DATA - SORTED LIST] Il numero massimo di tentativi %d per il blocco %lld è stato raggiunto\n", MOD_NAME, n, block->block_index);
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
    printk("%s: [PUT DATA - INSERIMENTO HT + SORTED] Segnalata la presenza per l'inserimento del blocco %lld\n", MOD_NAME, index);
#endif

    mutex_unlock(&inval_insert_mutex);

    /*
     * Durante l'inserimento di un nuovo blocco, non è posibile avere in esecuzione alcuna invalidazione.
     * Tuttavia, è possibile avere in parallelo molteplici inserimenti. Di conseguenza, la dimensione della
     * Sorted List può solo che aumentare durante gli inserimenti. Come primo passo, verifico se la lista è
     * attualmente vuota in modo da eseguire un inserimento in testa.
     */
    if(head_sorted_list == NULL)
    {
        ret = __sync_bool_compare_and_swap(&head_sorted_list, next, block);

        if(!ret)
        {

#ifdef NOT_CRITICAL_BUT_PUT
            printk("%s: [ERRORE PUT DATA - SORTED LIST] L'inserimento in coda non è stato eseguito poiché la lista non è più vuota\n", MOD_NAME);
#endif
            n++;
            goto no_empty;        
        }

#ifdef NOT_CRITICAL_BUT_PUT
        printk("%s: [PUT DATA - SORTED LIST] Inserimento in coda nella lista ordinata effettuato con successo per il blocco %lld\n", MOD_NAME, block->block_index);
#endif

        /* Comunico di aver terminato l'inserimento ad eventuali threads che devono eseguire una invalidazione */
        __sync_fetch_and_sub(&sync_var,1);

        wake_up_interruptible(&the_queue);

        return 0;
    }

no_empty:

    /*
     * Poiché le invalidazioni dei blocchi non possono essere eseguite
     * in concorrenza con gli inserimenti, da questo momento la lista
     * non potrà essere vuota. In questo modo, posso gestire correttamente
     * il puntatore 'sorted_list_next'della testa della lista.
     */

    if(n > 10)
    {
        printk("%s: [ERRORE PUT DATA - SORTED LIST] Numero tentativi massimo raggiunto per l'inserimento nella sorted list del blocco %lld\n", MOD_NAME, block->block_index);
         __sync_fetch_and_sub(&sync_var,1);
        wake_up_interruptible(&the_queue);
        return 1;
    }

    curr = head_sorted_list;

    /* Navigo la lista alla ricerca dell'ultimo elemento */
    while( curr->sorted_list_next != NULL )
    {
        curr = curr->sorted_list_next;
    }

    ret = __sync_bool_compare_and_swap(&(curr->sorted_list_next), next, block);

    /* Verifico se nel frattempo la coda della lista è stata modificata */

    if(!ret)
    {
#ifdef NOT_CRITICAL_BUT_PUT
        printk("%s: [ERRORE PUT DATA - SORTED LIST] Tentativo di inserimento #%d del blocco %lld terminato senza successo\n", MOD_NAME, n, block->block_index);
#endif
        n++;
        goto no_empty;
    }

    __sync_fetch_and_sub(&sync_var,1);

    wake_up_interruptible(&the_queue);

#ifdef NOT_CRITICAL_BUT_PUT
    printk("%s: [PUT DATA - SORTED LIST] Inserimento in coda nella lista ordinata effettuato con successo per il blocco %lld al tentativo %d\n", MOD_NAME, block->block_index, n);
#endif

    return 0;    
}


/**
 * insert_sorted_list - Inserisce un nuovo elemento all'interno della Sorted List
 *
 * @block: Il nuovo elemento da inserire all'interno della lista
 *
 * I blocchi validi vengono inseriti nella lista secondo l'ordine di consegna. La
 * struttura 'struct block' mantiene come metadato la posizione del blocco all'interno
 * della Sorted List. I blocchi devono essere posti all'interno della lista secondo
 * un ordine strettamente crescente del campo 'pos'.
 */
void insert_sorted_list(struct block *block)
{
    struct block *prev;
    struct block *curr;

    /* Verifico se il blocco deve essere inserito in testa alla Sorted List */

    if(head_sorted_list == NULL)
    {
        /* La lista è vuota ed eseguo un inserimento in testa */
        block->sorted_list_next = NULL;
        head_sorted_list = block;
    }
    else
    {
        /* Verifico se il blocco deve essere inserito in testa alla lista */

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




/**
 * init_sorted_list - Inizializzazione della struttura dati Sorted List
 *
 * @num_data_block: Numero dei blocchi di dati del dispositivo
 *
 * Utilizza la struttura dati della bitmask per determinare se un blocco è valido oppure
 * libero. In questo modo, evito di eseguire la sb_bread.
 *
 * Restituisce il valore 0 ser l'inizializzazione è andata a buon fine oppure 1 in caso
 * di fallimento.
 */
int init_sorted_list(uint64_t num_data_block)
{
    int isValid;
    uint64_t index;
    struct block *new_item;
    struct buffer_head *bh;
    struct soafs_block *data_block;
    struct soafs_sb_info *sbi;

    sbi = (struct soafs_sb_info *)sb_global->s_fs_info;

    /* Verifico se attualmente non esiste alcun blocco valido. */    

    if(sbi->num_block_free == (sbi->num_block - 2 - sbi->num_block_state))
    {
        printk("%s: [INIZIALIZZAZIONE CORE - SORTED LIST] Non ci sono blocchi attualmente validi\n", MOD_NAME);
        head_sorted_list = NULL;
        return 0;
    }

    bh = NULL;
    data_block = NULL;

    /*
     * Eseguo la scansione dei blocchi che sono attualmenti validi nel dispositivo.
     * Per evitare di eseguire inutilmente la sb_bread() quando il blocco è libero
     * utilizzo la struttura dati della bitmask che è presente nei blocchi di stato
     * del dispositivo.
     */

    for(index=0; index<num_data_block; index++)
    {
        /* Verifico la validità del blocco su cui sto iterando */
        isValid = check_bit(index);

        if(!isValid)
        {
            continue;
        }

        /* Leggo il contenuto del blocco per ricavare la sua posizione all'interno della lista */

        bh = sb_bread(sb_global, 2 + sbi->num_block_state + index);                   

        if(bh == NULL)
        {
            printk("%s: [ERRORE INIZIALIZZAZIONE CORE - SORTED LIST] Errore sb_bread() lettura del blocco %lld...\n", MOD_NAME, index);
            goto rollback_init_sorted;
        }

        data_block = (struct soafs_block *)bh->b_data;

        /* Alloco il nuovo elemento da inserire nella lista */

        new_item = (struct block *)kmalloc(sizeof(struct block), GFP_KERNEL);
    
        if(new_item == NULL)
        {
            printk("%s: [ERRORE INIZIALIZZAZIONE CORE - SORTED LIST] Errore esecuzione kmalloc()\n", MOD_NAME);
            goto rollback_init_sorted;
        }

        /* Inizializzo il nuovo elemento da inserire nella lista */

        new_item->block_index = index;
        new_item->pos = data_block->pos;

        insert_sorted_list(new_item);

#ifdef NOT_CRITICAL_BUT_INIT
        printk("%s: [INIZIALIZZAZIONE CORE - HT + SORTED] Il blocco di dati con indice %lld è valido e nella lista ordinata si trova in posizione %lld.\n", MOD_NAME, index, data_block->pos);
#endif
        brelse(bh);        
        
    }

    return 0;

rollback_init_sorted:

    while(head_sorted_list!=NULL)
    {
        new_item = head_sorted_list->sorted_list_next;
        kfree(head_sorted_list);
        head_sorted_list = new_item;
    }
    
    return 1;
}



/**
 * init_data_structure_core - Inizializzazione delle strutture dati core del modulo
 *
 * @num_data_block: Numero dei blocchi di dati del dispositivo
 * @index_free: array contenente gli indici dei blocchi liberi all'istante di montaggio
 * @actual_size: Numero degli indici dei blocchi liberi all'istante di montaggio nell'array
 *
 * Le strutture dati che vengono inizializzate sono la lista dei blocchi liberi e la lista
 * contenente gli indici dei blocchi disposti secondo l'ordine di consegna (Sorted List).
 * I messaggi contenuti all'interno dei blocchi non vengono caricati in memoria ma sono
 * recuperati su richiesta.
 *
 * Restituisce il valore 0 se l'inizializzazione è andata a buon fine oppure 1 in caso
 * di fallimento.
 */
int init_data_structure_core(uint64_t num_data_block, uint64_t *index_free, uint64_t actual_size)
{
    int ret;
    int i;
    uint64_t index;
    struct soafs_sb_info *sbi;
    struct block_free *roll_bf;

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

    /* Inizializzo la struttura dati Sorted List */
    ret = init_sorted_list(num_data_block);

    if(ret)
    {
        printk("%s: [ERRORE INIZIALIZZAZIONE CORE] L'inizializzazione della Sorted List non si è conclusa con successo\n", MOD_NAME);

        /* Dealloco la struttura dati free_block_list precedentemente allocata */

        for(index=0; index<actual_size; index++)
        {
            roll_bf = head_free_block_list->next;

            kfree(head_free_block_list);

            head_free_block_list = roll_bf;
        }

        if(head_free_block_list != NULL)
        {
            printk("%s: [ERRORE INIZIALIZZAZIONE CORE] Errore nella deallocazione della FREE LIST\n", MOD_NAME);
        }
        else
        {
            printk("%s: [ERRORE INIZIALIZZAZIONE CORE] Deallocazione della FREE LIST completata con successo\n", MOD_NAME);
        }

        return 1;
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
