/*
 * main.c
 */

#include <libfipc.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <thc.h>

MODULE_LICENSE("GPL");

static int __init abort_example_init(void)
{
	DO_FINISH(

		ASYNC(
			int i;
			printk(KERN_ERR "ASYNC 0 started\n");
			for (i = 0; i < 4; i++) {
				printk(KERN_ERR "ASYNC 0 scheduled\n");
			}

			printk(KERN_ERR "ASYNC 0 signaling all awe's\n");
			THCStopAllAwes();
			printk(KERN_ERR "ASYNC 0 aborting\n");
			THCAbort();
			
			);
		
		ASYNC(
			printk(KERN_ERR "ASYNC 1 started\n");
			while(!THCShouldStop())
				THCYield();
			printk(KERN_ERR "ASYNC 1 aborting\n");
			THCAbort();

			);

		ASYNC(

			printk(KERN_ERR "ASYNC 2 started\n");
			while(!THCShouldStop())
				THCYield();
			printk(KERN_ERR "ASYNC 2 aborting\n");
			THCAbort();

			);

		ASYNC(

			printk(KERN_ERR "ASYNC 3 started and aborting\n");
			THCAbort();

			);

		printk(KERN_ERR "Successfully ran code after async\n");

		// We will hit end of do-finish, which sets up an awe
		// for when all contained async's are done.
		);

	printk(KERN_ERR "Exited do finish\n");

        return 0;
}
static void __exit abort_example_rmmod(void)
{
	return;
}

module_init(abort_example_init);
module_exit(abort_example_rmmod);
