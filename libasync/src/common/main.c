/* 
 * main.c
 *
 * Authors: Anton Burtsev <aburtsev@flux.utah.edu>
 *          Muktesh Khole <muktesh.khole@utah.edu>
 * Copyright: University of Utah
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <thc.h>
#include <thcinternal.h>
#include <thcsync.h>
void foo1(void);
void foo2(void);

noinline void foo1(void) {
	printk(KERN_ERR "lcd async entering foo1\n");
	printk(KERN_ERR "lcd async yielding to foo2\n");
	int count = 0;
	while (count < 2) {
	 printk(KERN_ERR "lcd async Yielding\n");
	 THCYield();	
	 count++;
	}
	printk(KERN_ERR "lcd async foo1 complete\n");
}

noinline void foo2(void) {
	printk(KERN_ERR "lcd async entering foo2\n");
	printk(KERN_ERR "lcd async foo2 Complete\n");
}

static int __init api_init(void)
{
//	void ** frame = (void**)__builtin_frame_address(0);
//	void *ret_addr = *(frame + 1);
//	*(frame + 1) = NULL;
	current->ptstate = kzalloc(sizeof(struct ptstate_t), GFP_KERNEL);
	thc_latch_init(&(current->ptstate->latch));
	thc_init();
    //assert((PTS() == NULL) && "PTS already initialized");
	printk(KERN_ERR "lcd async entering module ptstate allocated");
	DO_FINISH(ASYNC(foo1();); printk(KERN_ERR "lcd async apit_init coming back\n"); ASYNC(foo2();););
    printk(KERN_ERR "lcd async end of DO_FINISH");
	thc_done();
	kfree(current->ptstate);
//	*(frame + 1) = ret_addr;
	return 0;
}

static void __exit api_exit(void)
{
	printk(KERN_ERR "lcd async exiting module");
	return;
}

module_init(api_init);
module_exit(api_exit);

