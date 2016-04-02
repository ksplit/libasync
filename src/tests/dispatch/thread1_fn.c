#include <thc.h>
#include <thc_ipc.h>
#include <libfipc.h>
#include <thcinternal.h>
#include "thc_dispatch_test.h"
#include "thread_fn_util.h"
#include "../test_helpers.h"
#include <awe_mapper.h>
#include <linux/delay.h>


#define BATCH_INTERVAL 100

static struct fipc_ring_channel* channel;
static volatile int num_responses = 0;

static void check_response(unsigned long lhs, 
        unsigned long rhs, 
        unsigned long response, 
        int fn_type)
{
    unsigned long result = 0;
    switch( fn_type )
    {
        case ADD_2_FN:
            result = lhs + rhs;
            break;
        case ADD_10_FN:
            result = lhs + rhs + 10;
            break;
        default:
            printk(KERN_ERR "invalid fn_type %d\n", fn_type);
            BUG();
            break;
    }

    if( response != result )
    {
        printk(KERN_ERR "response = %lu, result = %lu, fn_type = %d\n", response, result, fn_type);
        BUG();
    }
}

static inline int finish_response_check_fn_type_and_reg0(
	struct fipc_ring_channel *chnl,
	struct fipc_message *response,
	uint32_t expected_type,
	unsigned long expected_lhs,
	unsigned long expected_rhs)
{
	int ret;
	uint32_t actual_type      = get_fn_type(response);
	unsigned long actual_reg0 = fipc_get_reg0(response);
    unsigned long expected_reg0;

	ret = fipc_recv_msg_end(chnl, response);

    switch( expected_type )
    {
        case ADD_2_FN:
            expected_reg0 = expected_lhs + expected_rhs;
            break;
        case ADD_10_FN:
            expected_reg0 = expected_lhs + expected_rhs + 10;
            break;
        default:
            printk(KERN_ERR "invalid fn_type %d\n", expected_type);
            return -EINVAL;
    }

	if (ret) {
		pr_err("Error finishing receipt of response, ret = %d\n", ret);
		return ret;
	} else if (actual_type != expected_type) {
		pr_err("Unexpected fn type: actual = %u, expected = %u\n",
			actual_type, expected_type);
		return -EINVAL;
	} else if (actual_reg0 != expected_reg0) {
		pr_err("Unexpected return value (reg0): actual = 0x%lx, expected = 0x%lx\n",
			actual_reg0, expected_reg0);
		return -EINVAL;

	} else {
		return 0;
	}
}



static int add_nums_async(struct fipc_ring_channel* channel,
        unsigned long lhs, 
        unsigned long rhs, 
        int fn_type)
{
	struct fipc_message *msg;
    struct fipc_message *response;
	int ret;

	if( test_fipc_blocking_send_start(channel, &msg) )
    {
        printk(KERN_ERR "Error getting send message for add_nums_async.\n");
    }
	set_fn_type(msg, fn_type);
	fipc_set_reg0(msg, lhs);
	fipc_set_reg1(msg, rhs);

    ret = thc_ipc_call(channel, msg, &response);

  	if ( ret ) {
		printk(KERN_ERR "Error getting response, ret = %d\n", ret);
		goto fail;
	}

	return finish_response_check_fn_type_and_reg0(
		channel,
		response, 
		fn_type,
        lhs,
        rhs);

fail:
    return ret;
}


static int run_thread1(void* chan)
{
    volatile int num_transactions    = 0;
    int print_transactions_threshold = 0;
    unsigned long msg_response       = 0;
    num_responses = 0;
    channel       = chan;

    thc_init();
 	DO_FINISH_(thread1_fn,{
		while ( num_transactions < TRANSACTIONS ) {
        if( num_transactions >= print_transactions_threshold )
        {
            print_transactions_threshold += 300;
		    printk(KERN_ERR "num_transactions: %d\n", num_transactions);
        }

		ASYNC(
		    num_transactions++;
			msg_response = add_nums_async(channel, num_transactions, 1, ADD_2_FN);
            num_responses++;
		     );
        if( (num_transactions) % THD3_INTERVAL == 0 )
        {
            ASYNC(
		        num_transactions++;
                msg_response = add_nums_async(channel, num_transactions, 2, ADD_10_FN);
                num_responses++;
                 );
        }
		ASYNC(
		    num_transactions++;
			msg_response = add_nums_async(channel, num_transactions, 3, ADD_2_FN);
            num_responses++;
		     );
            //msleep(1);
	}
    printk(KERN_ERR "done with transactions\n");
    });
	printk(KERN_ERR "lcd async exiting module and deleting ptstate");
	printk(KERN_ERR "Expected number of responses %d, actual number of responses %d\n", num_transactions, num_responses);
	thc_done();
    
    if( num_responses != num_transactions )
    {
	    printk(KERN_ERR "Invalid number of responses\n");
        BUG();
    }

    return 0;
}


int thread1_fn(void* chan)
{
    int result;

    LCD_MAIN({result = run_thread1(chan);});

    return result;
}


