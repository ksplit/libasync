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

#define NUM_SWITCH_MEASUREMENTS 1000000

static unsigned long ctx_measurements_arr[NUM_SWITCH_MEASUREMENTS];
static unsigned long thd_measurements_arr[NUM_SWITCH_MEASUREMENTS];

static const int CPU_NUM = -1;


static int avg_thread_creation_test(void)
{
    unsigned long t1, t2;
    //NUM_ASYNCs should be set to equal the number of asyncs used per iteration.
    static const unsigned long long NUM_ASYNCS = 64;
    static const unsigned long long NUM_AWE_CREATION_ITERS = 100000;
    //number of iterations to average over
    unsigned long long awe_creation_time_sum = 0;
    int i;

    thc_init();
    for( i = 0; i < NUM_AWE_CREATION_ITERS; i++ )
    {
        DO_FINISH({
                t1 = test_fipc_start_stopwatch();
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                ASYNC({
                    t2 = test_fipc_stop_stopwatch();
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                    });
                });
            awe_creation_time_sum += t2 - t1;
            }
            thc_done();

            printk(KERN_ERR "Average time per AWE creation.\nUsing %llu iterations with %llu AWEs created per iteration (in cycles): %llu", 
                    NUM_AWE_CREATION_ITERS, 
                    NUM_ASYNCS, 
                    awe_creation_time_sum / (NUM_AWE_CREATION_ITERS*NUM_ASYNCS));
            return 0;
}

static int print_timing_data(void)
{
    printk(KERN_ERR "\nMore comprehensive approximate timing results\n for context switches (ignoring rdtsc timer overhead):\n");
    test_fipc_dump_time(ctx_measurements_arr, NUM_SWITCH_MEASUREMENTS);
    printk(KERN_ERR "\nMore comprehensive approximate timing results\n for thread creation (ignoring rdtsc timer overhead):\n");
    test_fipc_dump_time(thd_measurements_arr, NUM_SWITCH_MEASUREMENTS);
    printk(KERN_ERR "\n\n");
    
    return 0;
}

static int test_ctx_switch_and_thd_creation(void)
{
    unsigned long t1, t2, t3, t4;
    int ret;
    int id_1, id_2;
    int i = 0;

    thc_init();

        for( i = 0; i < NUM_SWITCH_MEASUREMENTS; i++ )
        {
            DO_FINISH_(ctx_switch,{
                    t1 = test_fipc_start_stopwatch();
                    ASYNC({
                        t2 = test_fipc_stop_stopwatch();
                        thd_measurements_arr[i] = t2 - t1;
                        ret = awe_mapper_create_id(&id_1);
                        THCYieldAndSave(id_1);
                        t4 = test_fipc_stop_stopwatch();
                        awe_mapper_remove_id(id_1);
                    });             
                    ASYNC({
                        t3 = test_fipc_start_stopwatch(); 
                        THCYieldToId(id_1);
                        });             
            });
            ctx_measurements_arr[i] = t4 - t3;
        }


            //Average time per context switch
            DO_FINISH_(ctx_switch,{

                    ASYNC({
                            int i;
                            ret = awe_mapper_create_id(&id_1);
                            THCYieldAndSave(id_1);
                            
                            for(i = 0; i < NUM_SWITCH_MEASUREMENTS / 2; i++)
                            {
                                THCYieldToIdAndSave(id_2,id_1);
                            }
                            t4 = test_fipc_stop_stopwatch();
                            awe_mapper_remove_id(id_1);
                            awe_mapper_remove_id(id_2);
                        });
                    ASYNC({
                            int i;
                            ret = awe_mapper_create_id(&id_2);
                            t3 = test_fipc_start_stopwatch(); 
                            for(i = 0; i < NUM_SWITCH_MEASUREMENTS / 2; i++)
                            {
                                THCYieldToIdAndSave(id_1,id_2);
                            }
                        });
                    });
                printk(KERN_ERR "Average time per context switch: %lu.", 
                                        (t4 - t3)/NUM_SWITCH_MEASUREMENTS);
                                        

    thc_done();
    print_timing_data();

    return 0;
}



static int test_ctx_switch_and_thd_creation_no_dispatch(void)
{
    unsigned long t1, t2, t3, t4;
    int ret;
    int id_1, id_2;
    int i = 0;
    thc_init();

        for( i = 0; i < NUM_SWITCH_MEASUREMENTS; i++ )
        {
            DO_FINISH_(ctx_switch,{
                    t1 = test_fipc_start_stopwatch();
                    ASYNC({
                        t2 = test_fipc_stop_stopwatch();
                        thd_measurements_arr[i] = t2 - t1;
                        ret = awe_mapper_create_id(&id_1);
                        THCYieldAndSaveNoDispatch(id_1);
                        t4 = test_fipc_stop_stopwatch();
                        awe_mapper_remove_id(id_1);
                    });             
                    ASYNC({
                        ret = awe_mapper_create_id(&id_2);
                        t3 = test_fipc_start_stopwatch(); 
                        THCYieldToIdAndSaveNoDispatch(id_1,id_2);
                        });             
                    //The Yield below is important since earlier we were relying on the dispatch queue
                    //to clean up AWEs, but now, extra care needs to be taken to clean them up.
                    //This is a hack for now, the semantics of not using a dispatch loop
                    //probably need to be hashed out more.
                    THCYieldToIdNoDispatch_TopLevel(id_2);
                    awe_mapper_remove_id(id_2);
            });
            ctx_measurements_arr[i] = t4 - t3;
        }

            DO_FINISH_(ctx_switch,{

                    ASYNC({
                            int i;
                            ret = awe_mapper_create_id(&id_1);
                            THCYieldAndSaveNoDispatch(id_1);
                            
                            for(i = 0; i < NUM_SWITCH_MEASUREMENTS / 2; i++)
                            {
                                THCYieldToIdAndSaveNoDispatch(id_2,id_1);
                            }
                            t4 = test_fipc_stop_stopwatch();
                            awe_mapper_remove_id(id_1);
                            awe_mapper_remove_id(id_2);
                        });
                    ASYNC({
                            int i;
                            ret = awe_mapper_create_id(&id_2);
                            t3 = test_fipc_start_stopwatch(); 
                            for(i = 0; i < NUM_SWITCH_MEASUREMENTS / 2; i++)
                            {
                                THCYieldToIdAndSaveNoDispatch(id_1,id_2);
                            }
                        });
                        THCYieldToIdNoDispatch_TopLevel(id_1);
                    });
                printk(KERN_ERR "Average time per context switch: %lu.", 
                                        (t4 - t3)/NUM_SWITCH_MEASUREMENTS);
    thc_done();
    print_timing_data();

    return 0;
}

static int test_fn(void)
{
    int ret;

    printk(KERN_ERR "Starting tests. Using %d samples per test.\n\n", 
                                            NUM_SWITCH_MEASUREMENTS);

    printk(KERN_ERR "\n\nTesting no dispatch yields\n");
    ret = test_fipc_run_test_fn(test_ctx_switch_and_thd_creation_no_dispatch, 
                                            CPU_NUM);
    printk(KERN_ERR "\n\nTesting with dispatch yields\n");
    ret |= test_fipc_run_test_fn(test_ctx_switch_and_thd_creation, 
                                            CPU_NUM);
    printk(KERN_ERR "\n\nTesting average thread creation time\n");
    ret |= test_fipc_run_test_fn(avg_thread_creation_test, 
                                            CPU_NUM);


    if( ret )
    {
        printk(KERN_ERR "Tests returned non-zero\n");
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
