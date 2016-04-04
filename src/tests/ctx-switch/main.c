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

#define NUM_SWITCH_MEASUREMENTS 100000
#define CPU_NUM 1

static unsigned long measurements_arr[NUM_SWITCH_MEASUREMENTS];

static int test_ctx_switch(void)
{
    unsigned long start_time, end_time;
    uint32_t msg_id;
    int i;    

    thc_init();
        for( i = 0; i < NUM_SWITCH_MEASUREMENTS; i++ )
        {
            DO_FINISH_(ctx_switch,{
                    ASYNC({
                        msg_id = awe_mapper_create_id();
                        THCYieldAndSave(msg_id);
                        end_time = test_fipc_stop_stopwatch();
                        awe_mapper_remove_id(msg_id);
                    });             
                    ASYNC({
                        start_time = test_fipc_start_stopwatch(); 
                        THCYieldToId(msg_id);
                        });             
            });
            measurements_arr[i] = end_time - start_time;
        }
    thc_done();

    return 0;
}


static int test_thread_creation(void)
{
    unsigned long start_time, end_time;

    int i;    

    thc_init();
    for( i = 0; i < NUM_SWITCH_MEASUREMENTS; i++ )
    {
            DO_FINISH_(thread_creation,{
                start_time = test_fipc_start_stopwatch(); 
                ASYNC({
                    end_time = test_fipc_stop_stopwatch();
                });             
            });

        measurements_arr[i] = end_time - start_time;
    }
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


static int run_test_fn(const char* result_str, int (*fn)(void))
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
    
    printk(KERN_ERR "%s\n", result_str);
    test_fipc_dump_time(measurements_arr, NUM_SWITCH_MEASUREMENTS);
    printk(KERN_ERR "\n\n");

    return 0;
fail:
    test_fipc_release_thread(thread);

    return ret;
}


static int test_fn(void)
{
    int ret;

    printk(KERN_ERR "Starting tests. Using %d samples per test.\n\n", NUM_SWITCH_MEASUREMENTS);

    ret = run_test_fn("Timing results for test_ctx_switch:", test_ctx_switch);

    if( ret )
    {
        printk(KERN_ERR "test_ctx_switch returned non-zero\n");
        return ret;
    }

    ret = run_test_fn("Timing results for test_thread_creation:", test_thread_creation);

    if( ret )
    {
        printk(KERN_ERR "test_thread_creation returned non-zero\n");
        return ret;
    }

    printk(KERN_ERR "Tests complete.\n\n");

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
