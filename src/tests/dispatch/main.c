#include <libfipc.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <thc_ipc_types.h>
#include <thc_ipc.h>

#include "../test_helpers.h"
#include "thread_fn_util.h"


#define THREAD1_CPU 1
#define THREAD2_CPU 2
#define THREAD3_CPU 3

#define CHANNEL_ORDER 2

MODULE_LICENSE("GPL");

//channel groups for threads 2 and 3
static struct thc_channel_group group2, group3;
static struct thc_channel_group_item item2to1, item2to3, item3;

static int setup_and_run_test(void)
{
	int ret;
	struct fipc_ring_channel *thread1_header, *thread_2_to_1_header;
    struct fipc_ring_channel *thread_2_to_3_header, *thread3_header;
	struct task_struct *thread1, *thread2, *thread3;

   /*
    * Initialize channel groups
    */
   thc_channel_group_init(&group2);
   thc_channel_group_init(&group3); 
   
   item2to1.channel = thread_2_to_1_header; 
   thc_channel_group_item_add(&group2, &item2to1); 

   item2to3.channel = thread_2_to_3_header; 
   thc_channel_group_item_add(&group2, &item2to3); 
  
   item3.channel = thread3_header;
   thc_channel_group_item_add(&group3, &item3); 

	/*
	 * Initialize fipc
	 */
	ret = fipc_init();
	if (ret) {
		pr_err("Error init'ing libfipc, ret = %d\n", ret);
		goto fail0;
	}
	/*
	 * Set up channel between thread 1 and 2
	 */
	ret = test_fipc_create_channel(CHANNEL_ORDER, &thread1_header, 
				&thread_2_to_1_header);
	if (ret) {
		pr_err("Error creating channel, ret = %d\n", ret);
		goto fail1;
	}

	/*
	 * Set up channel between thread 2 and 3
	 */
	ret = test_fipc_create_channel(CHANNEL_ORDER, &thread3_header, 
				&thread_2_to_3_header);
	if (ret) {
		pr_err("Error creating channel, ret = %d\n", ret);
		goto fail2;
	}
	



	/*
	 * Set up threads
	 */
	thread1 = test_fipc_spawn_thread_with_channel(thread1_header, 
							thread1_fn,
							THREAD1_CPU);
	if (!thread1) {
		pr_err("Error setting up caller thread\n");
		goto fail2;
	}
	thread2 = test_fipc_spawn_thread_with_channel(&group2, 
							thread2_fn,
							THREAD2_CPU);
	if (!thread2) {
		pr_err("Error setting up thread 2\n");
		goto fail3;
	}

	thread3 = test_fipc_spawn_thread_with_channel(&group3, 
							thread3_fn,
							THREAD3_CPU);
	if (!thread3) {
		pr_err("Error setting up thread 3\n");
		goto fail4;
	}



	/*
	 * Wake them up; they will run until they exit.
	 */
	wake_up_process(thread1);
	wake_up_process(thread2);
	wake_up_process(thread3);
	/* wait for a second so we don't prematurely kill caller/callee */
	msleep(1000); 
	/*
	 * Wait for them to complete, so we can tear things down
	 */
	ret = test_fipc_wait_for_thread(thread1);
	if (ret)
		pr_err("Caller returned non-zero exit status %d\n", ret);
	ret = test_fipc_wait_for_thread(thread2);
	if (ret)
		pr_err("Callee returned non-zero exit status %d\n", ret);

	ret = test_fipc_wait_for_thread(thread3);
	if (ret)
		pr_err("Callee returned non-zero exit status %d\n", ret);
	/*
	 * Tear things down
	 */
	test_fipc_free_channel(CHANNEL_ORDER, thread1_header, thread_2_to_1_header);
	test_fipc_free_channel(CHANNEL_ORDER, thread_2_to_3_header, thread3_header);

	fipc_fini();

	return 0;

 
fail4:
	test_fipc_release_thread(thread2);
	test_fipc_free_channel(CHANNEL_ORDER, thread_2_to_3_header, thread3_header);
fail3:
	test_fipc_release_thread(thread1);
fail2:
	test_fipc_free_channel(CHANNEL_ORDER, thread1_header, thread_2_to_1_header);
fail1:
	fipc_fini();
fail0:
	return ret;
}

static int __init rpc_init(void)
{
	int ret = 0;

	ret = setup_and_run_test();

        return ret;
}

static void __exit rpc_rmmod(void)
{
	return;
}

module_init(rpc_init);
module_exit(rpc_rmmod);
