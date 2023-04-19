#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0xcb440b5e, "module_layout" },
	{ 0xc83492ef, "kmalloc_caches" },
	{ 0xeb233a45, "__kmalloc" },
	{ 0x754d539c, "strlen" },
	{ 0xd9b85ef6, "lockref_get" },
	{ 0xf44235e8, "init_user_ns" },
	{ 0x3213f038, "mutex_unlock" },
	{ 0xbd49ebec, "mount_bdev" },
	{ 0x6448fba3, "d_add" },
	{ 0xf95e0fb3, "pv_ops" },
	{ 0xe2d5255a, "strcmp" },
	{ 0x6b10bee1, "_copy_to_user" },
	{ 0x3d9e418c, "__bread_gfp" },
	{ 0x9ec6ca96, "ktime_get_real_ts64" },
	{ 0x9166fada, "strncpy" },
	{ 0x4dfa8d4b, "mutex_lock" },
	{ 0x521fdd53, "set_nlink" },
	{ 0x49511c80, "sync_dirty_buffer" },
	{ 0x2f711446, "__brelse" },
	{ 0xfe487975, "init_wait_entry" },
	{ 0x800473f, "__cond_resched" },
	{ 0x87a21cb3, "__ubsan_handle_out_of_bounds" },
	{ 0xd0da656b, "__stack_chk_fail" },
	{ 0x1000e51, "schedule" },
	{ 0x92997ed8, "_printk" },
	{ 0x7b526d3b, "unlock_new_inode" },
	{ 0x1f934351, "kill_block_super" },
	{ 0x65487097, "__x86_indirect_thunk_rax" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0x8bd65ff4, "kmem_cache_alloc_trace" },
	{ 0x8fa2b863, "register_filesystem" },
	{ 0x3eeb2322, "__wake_up" },
	{ 0x8c26d495, "prepare_to_wait_event" },
	{ 0x96ab55b6, "iput" },
	{ 0x37a0cba, "kfree" },
	{ 0x525fc19d, "sb_set_blocksize" },
	{ 0x7fb63f4b, "d_make_root" },
	{ 0x92540fbf, "finish_wait" },
	{ 0xef1c81ff, "unregister_filesystem" },
	{ 0x13c49cc2, "_copy_from_user" },
	{ 0x7feccf4f, "param_ops_ulong" },
	{ 0x88db9f48, "__check_object_size" },
	{ 0xe9566df8, "iget_locked" },
	{ 0x5120dca4, "inode_init_owner" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "8CADEE1B770DCB6750EC006");
