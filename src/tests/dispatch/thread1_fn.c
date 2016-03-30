#include <thc.h>
#include <thc_ipc.h>
#include <libfipc.h>
#include <thcinternal.h>
#include "thc_dispatch_test.h"
#include "thread_fn_util.h"
#include "../test_helpers.h"
#include "rpc.h"
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

static unsigned long add_nums_async(unsigned long lhs, unsigned long rhs, unsigned long msg_id, int fn_type)
{
	struct fipc_message *msg;
    struct fipc_message *response;
	unsigned long result;
	if( test_fipc_blocking_send_start(channel, &msg) )
    {
        printk(KERN_ERR "Error getting send message for add_nums_async.\n");
    }
	set_fn_type(msg, fn_type);
	fipc_set_reg0(msg, lhs);
	fipc_set_reg1(msg, rhs);
	THC_MSG_ID(msg)   = msg_id;
    THC_MSG_TYPE(msg) = msg_type_request;
    send_and_get_response(channel, msg, &response, msg_id);
    fipc_recv_msg_end(channel, response);
	result = fipc_get_reg0(response);
	
	return result;
}


static int run_thread1(void* chan)
{
    volatile int num_transactions = 0;
    int print_transactions_threshold = 0;
    unsigned long msg_response    = 0;
	uint32_t id_num;
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
			id_num = awe_mapper_create_id();
		    num_transactions++;
            volatile int old_trans = num_transactions;
			msg_response = add_nums_async(old_trans, 1,(unsigned long) id_num, ADD_2_FN);
            check_response(old_trans, 1, msg_response, ADD_2_FN);
            num_responses++;
		     );
        if( (num_transactions) % THD3_INTERVAL == 0 )
        {
            ASYNC(
                id_num = awe_mapper_create_id();
		        num_transactions++;
                volatile int old_trans = num_transactions;
                msg_response = add_nums_async(old_trans, 2,(unsigned long) id_num, ADD_10_FN);
                check_response(old_trans, 2, msg_response, ADD_10_FN);
                num_responses++;
                 );
        }
		ASYNC(
			id_num = awe_mapper_create_id();
		    num_transactions++;
            volatile int old_trans = num_transactions;
			msg_response = add_nums_async(old_trans, 3,(unsigned long) id_num, ADD_2_FN);
            check_response(old_trans, 3, msg_response, ADD_2_FN);
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


