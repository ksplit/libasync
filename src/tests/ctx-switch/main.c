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

static unsigned long ctx_switch_measurements[NUM_SWITCH_MEASUREMENTS];


static int test_fn(void* data)
{
    unsigned long start_time, end_time, elapsed_time;
    uint32_t msg_id;


        preempt_disable();
        local_irq_disable();

            int i;    
            for( i = 0; i < NUM_SWITCH_MEASUREMENTS; i++ )
            {
                LCD_MAIN({
                thc_init();
                    DO_FINISH_(ctx_switch,{
                        ASYNC({
                            msg_id = awe_mapper_create_id();
                            //THCYieldAndSave(msg_id);
                            THCYield();
                            end_time = test_fipc_stop_stopwatch();
                        });             
                        ASYNC({
                            start_time = test_fipc_start_stopwatch(); 
                            //THCYieldToId(msg_id);
                            THCYield();
                            awe_mapper_remove_id(msg_id);
                            });             
                    });
                thc_done();

                ctx_switch_measurements[i] = end_time - start_time;
                });
            }
        preempt_enable();
        local_irq_enable();

        printk(KERN_ERR "Timing results of performing %d switches between awe contexts:\n", NUM_SWITCH_MEASUREMENTS);
        test_fipc_dump_time(ctx_switch_measurements, NUM_SWITCH_MEASUREMENTS);


    return 0;
}


static int setup_and_run_test(void)
{
    struct task_struct* thread = test_pin_to_core(NULL, test_fn, 1);
    int ret;
    wake_up_process(thread);
    msleep(1000);
    ret = test_fipc_wait_for_thread(thread);

	if (ret)
		printk(KERN_ERR "Thread returned non-zero exit status %d\n", ret);
	
    return ret;
}


static int __init ctx_switch_init(void)
{
	int ret = 0;

	ret = setup_and_run_test();

    return ret;
}


static void __exit ctx_switch_rmmod(void)
{
	return;
}


module_init(ctx_switch_init);
module_exit(ctx_switch_rmmod);
