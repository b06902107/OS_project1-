#include <linux/linkage.h>
#include <linux/kernel.h>

asmlinkage void sys_print(char *a)
{
	printk("%s", a);
	return;
}
