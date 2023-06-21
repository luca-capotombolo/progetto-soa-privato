#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/version.h>	/* For LINUX_VERSION_CODE */

#include "../headers/main_header.h"


ssize_t onefilefs_read(struct file * filp, char __user * buf, size_t len, loff_t * off) {

    int index;

    uint64_t curr_index;

    size_t bytes_copied;                
    size_t byte_to_copy_iter;           
    size_t len_msg;
                
    char *msg_to_copy;
                
    unsigned long ret;
    unsigned long my_epoch;

    struct soafs_block *b_data;
    struct soafs_super_block *b_data_sb;

    struct buffer_head *bh_b;
    struct buffer_head *bh_sb;

    struct soafs_sb_info *sbi;


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

    if(*off)
    {
        wake_up_umount();
        return 0;
    }

    my_epoch = __sync_fetch_and_add(&(gp->epoch_sorted),1);

    /* Recupero le informazioni FS specific */
    sbi = (struct soafs_sb_info *)sb_global->s_fs_info;

    /* Recupero il puntatore al superblocco del dispositivo che contiene l'indice del blocco in testa alla Sorted List */
    bh_sb = get_sb_block();

    if(bh_sb == NULL)
    {
        printk("%s: [ERRORE READ DRVIER] Errore lettura del superblocco\n", MOD_NAME);
        index = (my_epoch & MASK) ? 1 : 0;
        __sync_fetch_and_add(&(gp->standing_sorted[index]),1);
        wake_up_interruptible(&the_queue);
        wake_up_umount();
        return -EIO;
    }

    b_data_sb = (struct soafs_super_block *)bh_sb->b_data;

    if(b_data_sb == NULL)
    {
        printk("%s: [ERRORE READ DRVIER] Errore puntatore a NULL per il superblocco\n", MOD_NAME);
        index = (my_epoch & MASK) ? 1 : 0;
        __sync_fetch_and_add(&(gp->standing_sorted[index]),1);
        wake_up_interruptible(&the_queue);
        wake_up_umount();

        if(bh_sb != NULL)
            brelse(bh_sb);

        return -EIO;
    }

    /* Recupero l'indice del blocco in testa alla Sorted List che è contenuto nel superblocco del dispositivo */
    curr_index = b_data_sb->head_sorted_list;

    if(curr_index == sbi->num_block)
    {
        printk("%s: [READ DRIVER] Attualmente non ci sono messaggi da consegnare\n", MOD_NAME);
        index = (my_epoch & MASK) ? 1 : 0;
        __sync_fetch_and_add(&(gp->standing_sorted[index]),1);
        wake_up_interruptible(&the_queue);
        wake_up_umount();

        if(bh_sb != NULL)
            brelse(bh_sb);

        return 0;
    }

    /* Tengo traccia del numero di bytes che sono stati copiati */
    bytes_copied = 0;

    /* Alloco la memoria per il messaggio che deve essere copiato */
    msg_to_copy = (char *)kzalloc(len, GFP_KERNEL);

    if(msg_to_copy == NULL)
    {
        printk("%s: [ERRORE READ DRIVER] Errore nell'allocazione di memoria per il messaggio da copiare\n", MOD_NAME);
        index = (my_epoch & MASK) ? 1 : 0;
        __sync_fetch_and_add(&(gp->standing_sorted[index]),1);
        wake_up_interruptible(&the_queue);
        wake_up_umount();

        if(bh_sb != NULL)
            brelse(bh_sb);

        return -EIO;
    }

    while(curr_index != sbi->num_block)
    {

        /* Recupero il puntatore al blocco del dispositivo della Sorted List su cui sto iterando */
        bh_b = get_block(curr_index);

        if(bh_b == NULL)
        {
            printk("%s: [ERRORE READ DRVIER] Errore nel recupero del blocco %lld\n", MOD_NAME, curr_index);
            index = (my_epoch & MASK) ? 1 : 0;
            __sync_fetch_and_add(&(gp->standing_sorted[index]),1);
            wake_up_interruptible(&the_queue);
            kfree(msg_to_copy);
            wake_up_umount();

            if(bh_sb != NULL)
                brelse(bh_sb);

            return -EIO;
        }

        b_data = (struct soafs_block *)bh_b -> b_data;

        if(b_data == NULL)
        {
            printk("%s: [ERRORE READ DRVIER] Errore puntatore a NULL per il blocco %lld\n", MOD_NAME, curr_index);
            index = (my_epoch & MASK) ? 1 : 0;
            __sync_fetch_and_add(&(gp->standing_sorted[index]),1);
            wake_up_interruptible(&the_queue);
            kfree(msg_to_copy);
            wake_up_umount();

           if(bh_sb != NULL)
                brelse(bh_sb);

            if(bh_b != NULL)
                brelse(bh_b);

            return -EIO;
        }

        if(bytes_copied > len)
        {
            printk("%s: [ERRORE READ DRIVER] La quantità di byte copiati dalla Sorted List non è valida\n", MOD_NAME);
            index = (my_epoch & MASK) ? 1 : 0;
            __sync_fetch_and_add(&(gp->standing_sorted[index]),1);
            wake_up_interruptible(&the_queue);
            kfree(msg_to_copy);
            wake_up_umount();

           if(bh_sb != NULL)
                brelse(bh_sb);

            if(bh_b != NULL)
                brelse(bh_b);

            return -EIO;
        }  

        if(bytes_copied == len)
        {
            printk("%s: [READ DRIVER] Il contenuto del device richiesto è stato letto con successo\n", MOD_NAME);

           if(bh_sb != NULL)
                brelse(bh_sb);

            if(bh_b != NULL)
                brelse(bh_b);

            break;
        }
    
        /* Recupero la dimensione del messaggio contenuto all'interno del blocco su cui sto iterando */
        len_msg = b_data->dim;

        if( (bytes_copied + len_msg) > len)
        {
            byte_to_copy_iter = len - bytes_copied;                            

            if(byte_to_copy_iter > 0)
                memcpy(msg_to_copy + bytes_copied, b_data->msg, byte_to_copy_iter);

            bytes_copied += byte_to_copy_iter;

            break;
        }
        else
        {
            byte_to_copy_iter = len_msg;

            if(len_msg == 0)
            {
                printk("%s: [READ DRIVER] Il messaggio contenuto nel blocco corrente è vuoto\n", MOD_NAME);
                curr_index = b_data->next;
                continue;
            }

            memcpy(msg_to_copy + bytes_copied, b_data->msg, byte_to_copy_iter);
            bytes_copied += byte_to_copy_iter + 1;
            msg_to_copy[bytes_copied - 1] = '\n';      
        }

        curr_index = b_data->next;
    }

    index = (my_epoch & MASK) ? 1 : 0;
    __sync_fetch_and_add(&(gp->standing_sorted[index]),1);
    wake_up_interruptible(&the_queue);

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

    wake_up_umount();

    return bytes_copied - ret;
}




int onefilefs_open(struct inode *inode, struct file *file) {

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

    printk("%s: Il dispositivo è stato aperto\n", MOD_NAME);

    //wake_up_umount();

    return 0;
}




int onefilefs_release(struct inode *inode, struct file *file) {

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
    printk("%s: Il dispositivo è stato chiuso\n", MOD_NAME);

    //wake_up_umount();

    __sync_fetch_and_sub(&(num_threads_run),2);

    wake_up_interruptible(&umount_queue);

   	return 0;
}


struct dentry *onefilefs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags) {

    struct soafs_inode *FS_specific_inode;
    struct super_block *sb = parent_inode->i_sb;
    struct buffer_head *bh = NULL;
    struct inode *the_inode = NULL;

    if(!strcmp(child_dentry->d_name.name, SOAFS_UNIQUE_FILE_NAME)){
	
        the_inode = iget_locked(sb, 1);
        if (!the_inode)
        {
            printk("%s: Si è verificato un errore nel recupero dell'inode dell'unico file.", MOD_NAME);
       	    return ERR_PTR(-ENOMEM);
        }

	    if(!(the_inode->i_state & I_NEW))
        {
		    return child_dentry;
	    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 12, 0)
	    inode_init_owner(&init_user_ns,the_inode, NULL, S_IFREG);
#else
        inode_init_owner(the_inode, NULL, S_IFREG);
#endif

	    the_inode->i_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IXUSR | S_IXGRP | S_IXOTH;
        the_inode->i_fop = &soafs_file_operations;
	    the_inode->i_op = &soafs_inode_ops;

	    set_nlink(the_inode,1);

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
