/*
 * main.c
 *
 * This file is part of the benchmarking tests for libasync.
 * This file is responsible for measuring the context switch time.
 *
 * Author: Michael Quigley
 * Date: March 2016
 */

#include <libfipc.h>
#include <thc.h>
#include <thcinternal.h>
#include <awe_mapper.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/module.h>
#include "../test_helpers.h"

MODULE_LICENSE("GPL");

#define NUM_SWITCH_MEASUREMENTS 14
#define CPU_NUM 2

//static unsigned long ctx_measurements_arr[NUM_SWITCH_MEASUREMENTS];
//static unsigned long thd_measurements_arr[NUM_SWITCH_MEASUREMENTS];

static const int USE_OTHER_CORE = 0;

static int test_ctx_switch_and_thd_creation(void)
{
    unsigned long t1, t2, t3;
    uint32_t msg_id;
    int ret;

    int i = 0;
    thc_init();

        for( i = 0; i < NUM_SWITCH_MEASUREMENTS; i++ )
        {
            DO_FINISH_(ctx_switch,{
//                    t1 = test_fipc_start_stopwatch();
                    ASYNC({
  //                      t2 = test_fipc_stop_stopwatch();
                       // thd_measurements_arr[i] = t2 - t1;

                        ret = awe_mapper_create_id(&msg_id);
          //            THCYieldAndSave  
                        BUG_ON(ret);
                        THCYieldAndSave(msg_id);
                        //THCYield();

        //                t3 = test_fipc_stop_stopwatch();
                        awe_mapper_remove_id(msg_id);
                        printk(KERN_ERR "async 1 done\n");
                    });             
                    ASYNC({
      //                  t2 = test_fipc_start_stopwatch(); 
                        //BUG_ON(THCYieldToId(msg_id));
                        THCYieldToId(msg_id);
                        //THCYield();//To(awe_pointer);
                        printk(KERN_ERR "async 2 done\n");
                        });             
            });
        printk(KERN_ERR "outside of DO_FINISH\n");
    //        ctx_measurements_arr[i] = t3 - t2;
        }
        printk(KERN_ERR "outside of loop \n");
#if 0

	DO_FINISH_(ctx_switch,{
	    ASYNC({
		int i;
		for (i = 0; i < NUM_SWITCH_MEASUREMENTS; i++)
		  THCYield();
	      });
	    ASYNC({
		int i;
		for (i = 0; i < NUM_SWITCH_MEASUREMENTS; i++)
		  THCYield();
	      });
	  });
#endif
    thc_done();

    return 0;
}


static int thread_fn(void* data)
{
        int ret;

        preempt_disable();
        local_irq_disable();

        LCD_MAIN({ret = ((int (*)(void)) data)();});

        preempt_enable();
        local_irq_enable();
        
        return ret;
}
#if 0
static void print_max(unsigned long* arr)
{
    int max_iter = 0;
    unsigned long max_num  = 0;
    int i = 0;
    for(i = 0; i < NUM_SWITCH_MEASUREMENTS; i++)
    {
        if( arr[i] > max_num )
        {
            max_num = arr[i];
            max_iter = i;
        }
    }

    printk(KERN_ERR "\nmax_num = %lu, max iter = %d\n\n", max_num, max_iter);
}
#endif


static int run_test_fn_thread(int (*fn)(void))
{
    int ret; 
    struct task_struct* thread;

    thread = test_pin_to_core(fn, thread_fn, CPU_NUM);

    if( !thread )
    {
       printk(KERN_ERR "Error setting up thread.\n"); 
       goto fail;
    }

    wake_up_process(thread);
    msleep(1000);

    ret = test_fipc_wait_for_thread(thread);

    if( ret )
    {
       printk(KERN_ERR "Error waiting for thread.\n"); 
    }

/*    printk(KERN_ERR "\nTiming results for context switches:\n");
    print_max(ctx_measurements_arr);
    test_fipc_dump_time(ctx_measurements_arr, NUM_SWITCH_MEASUREMENTS);
    print_max(thd_measurements_arr);
    printk(KERN_ERR "\nTiming results for thread creation:\n");
    test_fipc_dump_time(thd_measurements_arr, NUM_SWITCH_MEASUREMENTS);
    printk(KERN_ERR "\n\n");*/

    return 0;
fail:
    test_fipc_release_thread(thread);

    return ret;
}



static int run_test_fn_nothread(int (*fn)(void))
{
    int ret; 

    ret = thread_fn(fn);

    if( ret )
    {
       printk(KERN_ERR "Error running test.\n"); 
       goto fail1;
    }
   /* 
    printk(KERN_ERR "\nTiming results for context switches:\n");
    print_max(ctx_measurements_arr);
    test_fipc_dump_time(ctx_measurements_arr, NUM_SWITCH_MEASUREMENTS);
    printk(KERN_ERR "\nTiming results for thread creation:\n");
    print_max(thd_measurements_arr);
    test_fipc_dump_time(thd_measurements_arr, NUM_SWITCH_MEASUREMENTS);
    printk(KERN_ERR "\n\n");
*/
    return 0;
fail1:
    return ret;
}




static int test_fn(void)
{
    int ret;

//    printk(KERN_ERR "Starting tests. Using %d samples per test.\n\n", NUM_SWITCH_MEASUREMENTS);

    if( USE_OTHER_CORE )
    {
        ret = run_test_fn_thread(test_ctx_switch_and_thd_creation);
    }
    else
    {
        ret = run_test_fn_nothread(test_ctx_switch_and_thd_creation);
    }

    if( ret )
    {
        printk(KERN_ERR "test_ctx_switch returned non-zero\n");
        return ret;
    }

    return ret;
}


static int __init ctx_switch_init(void)
{
	int ret = 0;

	ret = test_fn();

    return ret;
}


static void __exit ctx_switch_rmmod(void)
{
	return;
}


module_init(ctx_switch_init);
module_exit(ctx_switch_rmmod);
