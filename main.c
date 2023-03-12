#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>	/* Needed for KERN_INFO */
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/syscalls.h>
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


/*static int f(struct vfsmount *mnt, void *arg)
{

    struct super_block *sb;

    

    if(mnt == NULL)
        return 1;

    if(!mnt)
        return 1;

    sb = mnt->mnt_sb;

    if(strcmp("soafs", sb->s_type->name)==0)
        return 1;

    printk("%s: Tipologia %s.\n", MOD_NAME, sb->s_type->name);

    return 0;
}*/



/* static int get_path_file_0(void)
{
    char *path, *b;
    struct dentry *d;

    if(sb_global == NULL)
    {
        printk("%s: errore nella manipolazione del super blocco globale\n", MOD_NAME);
        return 0;
    }

    path = (char *)kmalloc(1000, GFP_KERNEL);

    if(path == NULL)
    {
        printk("Errore malloc.\n");
        return 0;
    }

    printk("%s: Cerca del file in un file system di tipo %s\n", MOD_NAME, sb_global->s_type->name);

    d = sb_global->s_root;

    printk("%s: nome della dentry %s\n", MOD_NAME, d->d_name.name);

    b = dentry_path_raw(d, path, 1000);

    printk("%s: b vale %s\n", MOD_NAME, b);
    
    return 1;
    
}



static int get_path_file_1(void)
{
    char *path_root, *path_pwd, *b_root, *b_pwd;
    struct path p_root, p_pwd;
    struct fs_struct *fs;

    if(sb_global == NULL)
    {
        printk("%s: errore nella manipolazione del super blocco globale\n", MOD_NAME);
        return 0;
    }

    path_root = (char *)kmalloc(1000, GFP_KERNEL);
    path_pwd = (char *)kmalloc(1000, GFP_KERNEL);

    if(path_root == NULL || path_pwd == NULL)
    {
        printk("Errore malloc.\n");
        return 0;
    }

    printk("%s: Cerca del file in un file system di tipo %s\n", MOD_NAME, sb_global->s_type->name);

    fs = current->fs;

    p_root = fs->root;

    p_pwd = fs->pwd;

    //b_root = d_absolute_path(&p_root, path_root, 1000);

    //printk("%s: p_root b vale %s\n", MOD_NAME, b_root);

    //b_pwd = d_absolute_path(&p_pwd, path_pwd, 1000);

    //printk("%s: p_root b vale %s\n", MOD_NAME, b_pwd);
    
    return 1;
    
}



static int get_path_file_2(void)
{
    char *path_root, *path_pwd, *b_root, *b_pwd;
    struct path p_root, p_pwd;
    struct fs_struct *fs;

    if(sb_global == NULL)
    {
        printk("%s: errore nella manipolazione del super blocco globale\n", MOD_NAME);
        return 0;
    }

    path_root = (char *)kmalloc(1000, GFP_KERNEL);
    path_pwd = (char *)kmalloc(1000, GFP_KERNEL);

    if(path_root == NULL || path_pwd == NULL)
    {
        printk("Errore malloc.\n");
        return 0;
    }

    printk("%s: Cerca del file in un file system di tipo %s\n", MOD_NAME, sb_global->s_type->name);

    fs = current->fs;

    p_root = fs->root;

    p_pwd = fs->pwd;

    b_root = d_path(&p_root, path_root, 1000);

    b_pwd = d_path(&p_pwd, path_pwd, 1000);

    printk("%s: b_root = %s\n", MOD_NAME, b_root);

    printk("%s: b_pwd = %s\n", MOD_NAME, b_pwd);
    
    return 1;
    
}



static int get_path_file_3(void)
{
    struct nsproxy *ns;
    struct mnt_namespace* mnt_ns;
    struct mount * root;
    struct dentry *mnt_mountpoint;

    ns = current->nsproxy;

    mnt_ns = ns->mnt_ns;

    root = mnt_ns->root;

    mnt_mountpoint = root->mnt_mountpoint;    

    return 1;
    
}*/



static int check_is_mounted(void)
{
    if(!is_mounted)
    {
        printk("%s: Il file system presente sul device non è stato montato.\n", MOD_NAME);
        return 0;
    }
    
    printk("Il file system presente sul device è stato montato.\n");

    return 1;
}



//TODO: Implementa la system call
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(1, _get_data, unsigned long, vaddr){
#else
asmlinkage int sys_get_data(unsigned long vaddr){
#endif

    int ret;
    //struct file *filp = NULL;
    
    printk("%s: Invocato la get_data.\n", MOD_NAME);

    ret = check_is_mounted();

    if(!ret)
    {
        return -ENODEV;
    }


    //TODO: Recuperare il path del file the-file.

    //filp = filp_open()

    /*
    if(IS_ERR(filp))
    {
        printk("%s: Errore apertura del file.\n", MOD_NAME);
        return -EIO;
    }
    */

    //vfs_read()

    //manipolazione dei dati per identificare il blocco, se esiste

    //filp_close()
    
    return 0;
	
}



//TODO: Implementa la system call
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(1, _put_data, unsigned long, vaddr){
#else
asmlinkage int sys_put_data(unsigned long vaddr){
#endif

    int ret;

    printk("%s: Invocato la put_data.\n", MOD_NAME);

    ret = check_is_mounted();

    printk("%s: Il device è stato montato su %s\n", MOD_NAME, mount_path);

    if(!ret)
    {
        return -ENODEV;
    }    

    return 0;
	
}



//TODO: Implementa la system call
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(1, _invalidate_data, unsigned long, vaddr){
#else
asmlinkage int sys_invalidate_data(unsigned long vaddr){
#endif

    int ret;

    printk("%s: Invocato la invalidate_data.\n", MOD_NAME);

    ret = check_is_mounted();

    if(!ret)
    {
        return -ENODEV;
    }

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
