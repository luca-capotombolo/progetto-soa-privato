#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/string.h>

#include "../headers/main_header.h"


ssize_t onefilefs_read(struct file * filp, char __user * buf, size_t len, loff_t * off) {

    //TODO: da implementare
    printk("%s: E' stata invocata la funzione di lettura.", MOD_NAME);
    return 1;

}


struct dentry *onefilefs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags) {

    struct soafs_inode *FS_specific_inode;
    struct super_block *sb = parent_inode->i_sb;
    struct buffer_head *bh = NULL;
    struct inode *the_inode = NULL;

    printk("%s: E' stata invocata la funzione di lookup per '%s'",MOD_NAME,child_dentry->d_name.name);

    if(!strcmp(child_dentry->d_name.name, SOAFS_UNIQUE_FILE_NAME)){
	
        the_inode = iget_locked(sb, 1);
        if (!the_inode)
        {
            printk("%s: Si è verificato un errore nel recupero dell'inode dell'unico file.", MOD_NAME);
       	    return ERR_PTR(-ENOMEM);
        }

	    if(!(the_inode->i_state & I_NEW))
        {
            printk("%s: L'inode dell'unico file è presente all'interno della cache.", MOD_NAME);
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
};
