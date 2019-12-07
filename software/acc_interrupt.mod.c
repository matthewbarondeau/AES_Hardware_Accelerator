#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x59d6e14e, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0x37a0cba, __VMLINUX_SYMBOL_STR(kfree) },
	{ 0xf20dabd8, __VMLINUX_SYMBOL_STR(free_irq) },
	{ 0xd6b8e852, __VMLINUX_SYMBOL_STR(request_threaded_irq) },
	{ 0x192bf643, __VMLINUX_SYMBOL_STR(remove_proc_entry) },
	{ 0x6bc3fbc0, __VMLINUX_SYMBOL_STR(__unregister_chrdev) },
	{ 0x76f6c5ef, __VMLINUX_SYMBOL_STR(kmalloc_order) },
	{ 0x3ce59a59, __VMLINUX_SYMBOL_STR(proc_create_data) },
	{ 0x6eb97341, __VMLINUX_SYMBOL_STR(__register_chrdev) },
	{ 0xa2c0d80, __VMLINUX_SYMBOL_STR(__platform_driver_register) },
	{ 0xdcde339d, __VMLINUX_SYMBOL_STR(platform_driver_unregister) },
	{ 0x2712c7f, __VMLINUX_SYMBOL_STR(platform_get_resource) },
	{ 0xf5bcf528, __VMLINUX_SYMBOL_STR(fasync_helper) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0xbe67aae8, __VMLINUX_SYMBOL_STR(kill_fasync) },
	{ 0xefd6cf06, __VMLINUX_SYMBOL_STR(__aeabi_unwind_cpp_pr0) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

MODULE_ALIAS("of:N*T*Cxlnx,accelerator-lab-3-1.0");
MODULE_ALIAS("of:N*T*Cxlnx,accelerator-lab-3-1.0C*");
