#define EXPORT_SYMTAB
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/kprobes.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <asm/page.h>
#include <asm/cacheflush.h>
#include <asm/apic.h>
#include <asm/io.h>
#include <linux/syscalls.h>
#include "lib/include/scth.h"

#define MODNAME "SOA_SYSTEM_CALLS"
#define HACKED_ENTRIES (int)(sizeof(new_sys_call_array)/sizeof(unsigned long))
#define AUDIT if(1)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Luca Capotombolo <capoluca99@gmail.com>");


unsigned long the_syscall_table = 0x0;

module_param(the_syscall_table, ulong, 0660);

unsigned long the_ni_syscall;

unsigned long new_sys_call_array[] = {0x0, 0x0, 0x0};

int restore[HACKED_ENTRIES] = {[0 ... (HACKED_ENTRIES-1)] -1};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(1, _get_data, unsigned long, vaddr){
#else
asmlinkage int sys_get_data(unsigned long vaddr){
#endif

    //TODO: Implementa la system call
    printk("%s: Invocato la get_data.\n", MODNAME);
    return 0;
	
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(1, _put_data, unsigned long, vaddr){
#else
asmlinkage int sys_put_data(unsigned long vaddr){
#endif

    //TODO: Implementa la system call
    printk("%s: Invocato la put_data.\n", MODNAME);
    return 0;
	
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(1, _invalidate_data, unsigned long, vaddr){
#else
asmlinkage int sys_invalidate_data(unsigned long vaddr){
#endif

    //TODO: Implementa la system call
    printk("%s: Invocato la invalidate_data.\n", MODNAME);
    return 0;
	
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
long sys_get_data = (unsigned long) __x64_sys_get_data;
long sys_put_data = (unsigned long) __x64_sys_put_data;
long sys_invalidate_data = (unsigned long) __x64_sys_invalidate_data;       
#else
#endif


int init_module(void)
{

    int i;
    int ret;

	AUDIT
    {
        printk("%s: received sys_call_table address %px\n",MODNAME,(void*)the_syscall_table);
     	printk("%s: initializing - hacked entries %d\n",MODNAME,HACKED_ENTRIES);
	}

	new_sys_call_array[0] = (unsigned long)sys_get_data;
    new_sys_call_array[1] = (unsigned long)sys_put_data;
    new_sys_call_array[2] = (unsigned long)sys_invalidate_data;

    ret = get_entries(restore,HACKED_ENTRIES,(unsigned long*)the_syscall_table,&the_ni_syscall);

    if (ret != HACKED_ENTRIES)
    {
        printk("%s: could not hack %d entries (just %d)\n",MODNAME,HACKED_ENTRIES,ret); 
        return -1;      
    }

	unprotect_memory();

    for(i=0;i<HACKED_ENTRIES;i++)
    {
        ((unsigned long *)the_syscall_table)[restore[i]] = (unsigned long)new_sys_call_array[i];
    }

	protect_memory();

    printk("%s: all new system-calls correctly installed on sys-call table\n",MODNAME);

    return 0;

}

void cleanup_module(void)
{

    int i;
                
    printk("%s: shutting down\n",MODNAME);

	unprotect_memory();

    for(i=0;i<HACKED_ENTRIES;i++)
    {
        ((unsigned long *)the_syscall_table)[restore[i]] = the_ni_syscall;
    }

	protect_memory();

    printk("%s: sys-call table restored to its original content\n",MODNAME);
        
}
