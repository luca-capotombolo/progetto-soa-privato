#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/string.h>
#include <linux/slab.h>

#include "../headers/main_header.h"


ssize_t onefilefs_read(struct file * filp, char __user * buf, size_t len, loff_t * off) {

    struct block *curr;
    size_t bytes_copied;                /* Numero di bytes dei messaggi che sono stati già copiati */
    size_t byte_to_copy_iter;           /* Numero di bytes che devono essere copiati nella iterazione corrente */
    size_t len_msg;                     /* Dimensione del messaggio su cui si sta attualmente iteranod */
    char *msg_to_copy;                  /* Messaggio che deve essere restituito al'utente */
    unsigned long ret;
    //grace period
    unsigned long my_epoch;
    int index;
    
    printk("%s: [READ DRIVER] E' stata invocata la funzione di lettura con la dimensione richiesta pari a %ld.", MOD_NAME, len);

    if(*off)
    {
        return 0;
    }

    my_epoch = __sync_fetch_and_add(&(gp->epoch_sorted),1);

    curr = head_sorted_list;

    if(curr == NULL)
    {
        printk("%s: [READ DRIVER] Attualmente non ci sono messaggi da consegnare\n", MOD_NAME);

        index = (my_epoch & MASK) ? 1 : 0;

        __sync_fetch_and_add(&(gp->standing_sorted[index]),1);

        wake_up_interruptible(&the_queue);

        return 0;
    }

    bytes_copied = 0;

    msg_to_copy = (char *)kzalloc(len, GFP_KERNEL);

    if(msg_to_copy == NULL)
    {
        printk("%s: [ERRORE READ DRIVER] Errore esecuzione kzalloc() durante l'esecuzione della read()\n", MOD_NAME);

        index = (my_epoch & MASK) ? 1 : 0;

        __sync_fetch_and_add(&(gp->standing_sorted[index]),1);

        wake_up_interruptible(&the_queue);

        return -EIO;
    }

    while(curr != NULL)
    {

        if(bytes_copied > len)
        {
            printk("%s: [ERRORE READ DRIVER] Quantità di byte copiati non valida\n", MOD_NAME);

            index = (my_epoch & MASK) ? 1 : 0;

            __sync_fetch_and_add(&(gp->standing_sorted[index]),1);

            wake_up_interruptible(&the_queue);

            kfree(msg_to_copy);

            return -EIO;
        }  

        if(bytes_copied == len)
        {
            /* La quantità di richiesta dall'utente è stata copiata con successo */
            printk("%s: [READ DRIVER] Il contenuto del device richiesto è stato letto con successo\n", MOD_NAME);
            break;
        }
    
        len_msg = strlen(curr->msg);

        if( (bytes_copied + len_msg + 1) >= len)
        {
            byte_to_copy_iter = len - bytes_copied - 1;                            /* Tengo conto anche del terminatore di stringa */

            if(byte_to_copy_iter > 0)
            {
                strncpy(msg_to_copy + bytes_copied, curr->msg, byte_to_copy_iter);
            }

            bytes_copied += byte_to_copy_iter + 1;

            msg_to_copy[bytes_copied - 1] = '\0';

            break;
        }
        else
        {
            byte_to_copy_iter = len_msg;

            if(len_msg == 0)
            {
                printk("%s: [READ DRIVER] Messaggio vuoto\n", MOD_NAME);
                curr = curr->sorted_list_next;
                continue;
            }

            strncpy(msg_to_copy + bytes_copied, curr->msg, byte_to_copy_iter);

            bytes_copied += byte_to_copy_iter + 1;

            msg_to_copy[bytes_copied - 1] = '\n';      
        }

        curr = curr->sorted_list_next;
    }

    msg_to_copy[bytes_copied - 1] = '\0';

    if(curr == NULL)
    {
        /* EOF: Ho iterato su tutti i messaggi validi */
        printk("%s: [READ DRIVER] Il contenuto del device è stato letto completamente con successo\n", MOD_NAME);
    }

    index = (my_epoch & MASK) ? 1 : 0;

    __sync_fetch_and_add(&(gp->standing_sorted[index]),1);

    wake_up_interruptible(&the_queue);

    printk("%s: [READ DRIVER] Dimensione del messaggio da consegnare all'utente è pari a %ld\n", MOD_NAME, strlen(msg_to_copy) + 1);

    printk("%s [READ DRIVER] Numero di bytes che sono stati letti dal device è pari a %ld\n", MOD_NAME, bytes_copied);

    if(bytes_copied > 0)
    {
        ret = copy_to_user(buf, msg_to_copy, bytes_copied);
    }
    else
    {
        printk("%s: [ERRORE READ DRIVER] Il numero di byte copiati dal device è pari a 0\n", MOD_NAME);
        ret = 0;
    }

    *off = 1;

    return bytes_copied - ret;

}



int onefilefs_open(struct inode *inode, struct file *file) {

  	 return 0;
}



int onefilefs_release(struct inode *inode, struct file *file) {

   	return 0;
}


struct dentry *onefilefs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags) {

    struct soafs_inode *FS_specific_inode;
    struct super_block *sb = parent_inode->i_sb;
    struct buffer_head *bh = NULL;
    struct inode *the_inode = NULL;

    //printk("%s: E' stata invocata la funzione di lookup per '%s'",MOD_NAME,child_dentry->d_name.name);

    if(!strcmp(child_dentry->d_name.name, SOAFS_UNIQUE_FILE_NAME)){
	
        the_inode = iget_locked(sb, 1);
        if (!the_inode)
        {
            printk("%s: Si è verificato un errore nel recupero dell'inode dell'unico file.", MOD_NAME);
       	    return ERR_PTR(-ENOMEM);
        }

	    if(!(the_inode->i_state & I_NEW))
        {
            //printk("%s: L'inode dell'unico file è presente all'interno della cache.", MOD_NAME);
		    return child_dentry;
	    }

	    inode_init_owner(&init_user_ns,the_inode, NULL, S_IFREG);
	    the_inode->i_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IXUSR | S_IXGRP | S_IXOTH;
        the_inode->i_fop = &soafs_file_operations;
	    the_inode->i_op = &soafs_inode_ops;

	    //just one link for this file
	    set_nlink(the_inode,1);

	    //now we retrieve the file size via the FS specific inode, putting it into the generic inode
    	bh = (struct buffer_head *)sb_bread(sb, SOAFS_INODE_BLOCK_NUMBER);

    	if(bh==NULL)
        {
            printk("%s: Errore nel recupero del blocco contenente l'inode.", MOD_NAME);
		    iput(the_inode);
		    return ERR_PTR(-EIO);
    	}

	    FS_specific_inode = (struct soafs_inode*)bh->b_data;

	    the_inode->i_size = FS_specific_inode->file_size;

        brelse(bh);

        /* Associo la dentry 'child_dentry' con l'inode 'the_inode'. */
        d_add(child_dentry, the_inode);

        /* Incrementa il reference count. */
	    dget(child_dentry);

    	unlock_new_inode(the_inode);

	    return child_dentry;
    }

    return NULL;

}

const struct inode_operations soafs_inode_ops = {
    .lookup = onefilefs_lookup,
};

const struct file_operations soafs_file_operations = {
    .owner = THIS_MODULE,
    .read = onefilefs_read,
    .open = onefilefs_open,
    .release = onefilefs_release,
};
