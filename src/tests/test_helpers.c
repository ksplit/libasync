#include "test_helpers.h"
#include <linux/delay.h>



static int test_fipc_compare(const void *_a, const void *_b){

	u64 a = *((u64 *)_a);
	u64 b = *((u64 *)_b);

	if(a < b)
		return -1;
	if(a > b)
		return 1;
	return 0;
}


void test_fipc_dump_time(unsigned long *time, unsigned long num_transactions)
{

	int i;
	unsigned long long counter = 0;
    unsigned long min;
	unsigned long max;

	for (i = 0; i < num_transactions; i++) {
		counter+= time[i];
	}

	sort(time, num_transactions, sizeof(unsigned long), test_fipc_compare, NULL);

	min = time[0];
	max = time[num_transactions-1];
	//counter = min;

	printk(KERN_ERR "MIN\tMAX\tAVG\tMEDIAN\n");
	printk(KERN_ERR "%lu & %lu & %llu & %lu \n", min, max, counter/num_transactions, time[num_transactions/2]);
}

static int test_fipc_thread_fn(void* data)
{
        int ret;

        preempt_disable();
        local_irq_disable();

        LCD_MAIN({ret = ((int (*)(void)) data)();});

        preempt_enable();
        local_irq_enable();
        
        return ret;
}

static int test_fipc_run_test_fn_thread(int (*fn)(void), int cpu_num)
{
    int ret; 
    struct task_struct* thread;

    thread = test_pin_to_core(fn, test_fipc_thread_fn, cpu_num);

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


    return 0;
fail:
    test_fipc_release_thread(thread);

    return ret;
}

static int test_fipc_run_test_fn_nothread(int (*fn)(void))
{
    int ret; 

    ret = test_fipc_thread_fn(fn);

    if( ret )
    {
       printk(KERN_ERR "Error running test.\n"); 
       goto fail1;
    }

    return 0;
fail1:
    return ret;
}

/*run_test_fn
 Runs provided function 'fn' with the appropriate setup 
 (irq and preemption disabled during execution)
 if core_num is -1, then the provided function is run 
 on the current core, otherwise, it is run
 in a kthread on core 'core_num'.*/
int test_fipc_run_test_fn(int (*fn)(void), int core_num)
{
        return (core_num == -1) ? 
            test_fipc_run_test_fn_nothread(fn) 
            : test_fipc_run_test_fn_thread(fn, core_num);
}


