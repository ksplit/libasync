/*
 * main.c
 */

#include <libfipc.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <thc.h>
#include "../test_helpers.h"

MODULE_LICENSE("GPL");

static void abort_example_init(void)
{
	DO_FINISH(

		ASYNC(
			int i;
			printk(KERN_ERR "ASYNC 0 started\n");
			for (i = 0; i < 4; i++) {
				THCYield();
				printk(KERN_ERR "ASYNC 0 scheduled\n");
			}

			printk(KERN_ERR "ASYNC 0 signaling all awe's\n");
			THCStopAllAwes();
			printk(KERN_ERR "ASYNC 0 aborting\n");
			THCAbort();
			
			);
		
		ASYNC(
			printk(KERN_ERR "ASYNC 1 started\n");
			while(!THCShouldStop()) {
				THCYield();
				printk(KERN_ERR "ASYNC 1 Scheduled\n");
			}
			printk(KERN_ERR "ASYNC 1 aborting\n");
			THCAbort();

			);

		ASYNC(

			printk(KERN_ERR "ASYNC 2 started\n");
			while(!THCShouldStop()) {
				THCYield();
				printk(KERN_ERR "Async 2 Scheduled\n");
			}
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

}

static int __abort_example_init(void)
{
	thc_init();

	LCD_MAIN( abort_example_init(); );

	thc_done();

	return 0;
}

static void __exit abort_example_rmmod(void)
{
	return;
}

module_init(__abort_example_init);
module_exit(abort_example_rmmod);
