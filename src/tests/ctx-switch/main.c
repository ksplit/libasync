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
#include <linux/kernel.h>
#include <linux/module.h>
#include "../test_helpers.h"

MODULE_LICENSE("GPL");

#define NUM_SWITCH_MEASUREMENTS 1000

static unsigned long ctx_switch_measurements[NUM_SWITCH_MEASUREMENTS];


static int setup_and_run_test(void)
{
    unsigned long start_time, end_time, elapsed_time;
    uint32_t msg_id;

    thc_init();
    
    for( int i = 0; i < NUM_SWITCH_MEASUREMENTS; i++ )
    {
        DO_FINISH_(ctx_switch,{
            ASYNC({
                msg_id = awe_mapper_create_id();
                THCYieldAndSave(msg_id);
                end_time = test_fipc_stop_stopwatch();
            });             
            ASYNC({
                start_time = test_fipc_start_stopwatch(); 
                THCYieldToId(msg_id);
                awe_mapper_remove_id(msg_id);
                });             
        });

        ctx_switch_measurements[i] = end_time - start_time;
    }
    
    printk(KERN_ERR "Timing results of performing %d switches between awe contexts:\n", NUM_SWITCH_MEASUREMENTS);
    test_fipc_dump_time(ctx_switch_measurements, NUM_SWITCH_MEASUREMENTS);

    thc_done();
}


static int __init ctx_switch_init(void)
{
	int ret = 0;

	LCD_MAIN({ ret = setup_and_run_test(); });

    return ret;
}


static void __exit ctx_switch_rmmod(void)
{
	return;
}


module_init(ctx_switch_init);
module_exit(ctx_switch_rmmod);
