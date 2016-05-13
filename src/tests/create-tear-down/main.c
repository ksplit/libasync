/*
 * main.c
 *
 * This file is part of the benchmarking tests for libasync.
 * This file is responsible for measuring the awe create/tear-down time.
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

#define NUM_ITERS 100000

static const int CPU_NUM = -1;

static unsigned long id_1, id_2;

/*dummy_ret_val prevents the call to foo() from being optimized out.*/
static volatile int dummy_ret_val = 0;

__attribute__((used)) noinline static int foo_yieldto(void)
{
    THCYieldToIdAndSave(id_2, id_1);

    return 0;
}

__attribute__((used)) noinline static int foo_yield(void)
{
    THCYield();

    return 0;
}

__attribute__((used)) noinline static volatile int foo(void)
{
    return dummy_ret_val;
}

static int test_create_and_tear_down(void)
{
    unsigned long t1, t2;
    int ret = 0;
    int i = 0;


    thc_init();
    awe_mapper_create_id(&id_1);
    awe_mapper_create_id(&id_2);


    t1 = test_fipc_start_stopwatch();
    for( i = 0; i < NUM_ITERS; i++ )
    {
        foo();
    }
    t2 = test_fipc_stop_stopwatch();
    printk(KERN_ERR "Cycles for empty function invocation: %lu\n", (t2 - t1)/NUM_ITERS);    

    t1 = test_fipc_start_stopwatch();
    DO_FINISH({
        for( i = 0; i < NUM_ITERS; i++ )
        {
            ASYNC(foo(););
        }
    });
    t2 = test_fipc_stop_stopwatch();
    printk(KERN_ERR "Cycles for empty function invocation in ASYNC: %lu\n", (t2 - t1)/NUM_ITERS);   

    t1 = test_fipc_start_stopwatch();
    DO_FINISH({
        for( i = 0; i < NUM_ITERS; i++ )
        {
            ASYNC(foo_yield(););
            THCYield();
        }
    });
    t2 = test_fipc_stop_stopwatch();
    printk(KERN_ERR "Cycles for blocking function invocation in ASYNC (Yield): %lu\n", (t2 - t1)/NUM_ITERS);   

    t1 = test_fipc_start_stopwatch();
    DO_FINISH({
        THCYieldAndSave(id_2);
        for( i = 0; i < NUM_ITERS; i++ )
        {
            ASYNC(foo_yieldto(););
            THCYieldToIdAndSave(id_1, id_2);
        }
    });
    t2 = test_fipc_stop_stopwatch();

    awe_mapper_remove_id(id_1);
    awe_mapper_remove_id(id_2);
    printk(KERN_ERR "Cycles for blocking function invocation in ASYNC (YieldTo): %lu\n", (t2 - t1)/NUM_ITERS);   


    thc_done();

    return ret;
}

static int test_fn(void)
{
    int ret = 0;

    printk(KERN_ERR "Starting tests. Using %d iterations per test.\n\n", 
                                            NUM_ITERS);

    ret = test_fipc_run_test_fn(test_create_and_tear_down, CPU_NUM);

    if( ret )
    {
        printk(KERN_ERR "Tests returned non-zero\n");
    }

    return ret;
}


static int __init awe_create_init(void)
{
	int ret = 0;

	ret = test_fn();

    return ret;
}


static void __exit awe_create_rmmod(void)
{
	return;
}


module_init(awe_create_init);
module_exit(awe_create_rmmod);
