#include <linux/linkage.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/uaccess.h>

asmlinkage int sys_get_now_time(unsigned long *a, unsigned long *b){
	struct timespec t;
	getnstimeofday(&t);
	copy_to_user(a, &t.tv_sec, sizeof(unsigned long));
	copy_to_user(b, &t.tv_nsec, sizeof(unsigned long));
	return 0;
}
