#include <thc.h>
#include <libfipc.h>
#include <thcinternal.h>
#include <thc_ipc.h>
#include <thc_ipc_types.h>
#include "thc_dispatch_test.h"
#include "../test_helpers.h"
#include "thread_fn_util.h"
#include <awe_mapper.h>
#define THREAD2_FNS_LENGTH 2

static struct thc_channel_group* rx_group;

//Just returns a value back to thread 1
static int add_2_fn(struct fipc_ring_channel* chan, struct fipc_message* msg)
{
    unsigned long result = fipc_get_reg0(msg) + fipc_get_reg1(msg);
	struct fipc_message* out_msg;
    uint32_t request_cookie = thc_get_request_cookie(msg);

    fipc_recv_msg_end(chan, msg);

	if( test_fipc_blocking_send_start(chan, &out_msg) )
    {
        printk(KERN_ERR "Error getting send message for add_2_fn.\n");
    }

    fipc_set_reg0(out_msg,result);
    set_fn_type(out_msg, ADD_2_FN);

    if( thc_ipc_reply(chan, request_cookie, out_msg) )
    {
        printk(KERN_ERR "Error sending message for add_2_fn.\n");
    }

    
    return 0;
}


//Receives a value from thread1, then passes it to thread 3 and returns that result to thread 1
static int add_10_fn(struct fipc_ring_channel* thread1_chan, struct fipc_message* msg)
{
    struct thc_channel_group_item *thread3_item;
 	struct fipc_message* thread1_result;
 	struct fipc_message* thread3_request;
 	struct fipc_message* thread3_response;
    unsigned long saved_msg_id = thc_get_msg_id(msg);
    struct fipc_ring_channel* thread3_chan;

    if( thc_channel_group_item_get(rx_group, 1, &thread3_item) )
    {
        printk(KERN_ERR "invalid index for group_item_get\n");
        return 1;
    }
    thread3_chan = thread3_item->channel;

	if( test_fipc_blocking_send_start(thread3_chan, &thread3_request) )
    {
        printk(KERN_ERR "Error getting send message for add_10_fn.\n");
    }

	set_fn_type(thread3_request, get_fn_type(msg));
	fipc_set_reg0(thread3_request, fipc_get_reg0(msg));
	fipc_set_reg1(thread3_request, fipc_get_reg1(msg));

    //mark channel 1 message as received and the slot as available
    fipc_recv_msg_end(thread1_chan, msg);

    thc_ipc_call(thread3_chan, thread3_request, &thread3_response);
    fipc_recv_msg_end(thread3_chan, msg);

    if( test_fipc_blocking_send_start(thread1_chan, &thread1_result) )
    {
        printk(KERN_ERR "Error getting send message for add_10_fn.\n");
    }

	set_fn_type(thread1_result, get_fn_type(thread3_response));
	fipc_set_reg0(thread1_result, fipc_get_reg0(thread3_response));
	fipc_set_reg1(thread1_result, fipc_get_reg1(thread3_response));

    if( thc_ipc_reply(thread1_chan, saved_msg_id , thread1_result) )
    {
        printk(KERN_ERR "Error sending message for add_10_fn.\n");
    }
    
    return 0;
}


static int thread1_dispatch_fn(struct fipc_ring_channel* chan, struct fipc_message* msg)
{
    switch( get_fn_type(msg) )
    {
        case ADD_2_FN:
            return add_2_fn(chan, msg);
        case ADD_10_FN:
            return add_10_fn(chan, msg);
        default:
            printk(KERN_ERR "FN: %d is not a valid function type\n", get_fn_type(msg));
    }
    return 1;
}

int thread2_fn(void* group)
{
    struct thc_channel_group_item *thrd1_item;
    thc_init();
    rx_group = (struct thc_channel_group*)group;
    thc_channel_group_item_get(rx_group, 0, &thrd1_item);
    thrd1_item->dispatch_fn = thread1_dispatch_fn;
    LCD_MAIN(thc_dispatch_loop_test(rx_group, TRANSACTIONS + TRANSACTIONS/THD3_INTERVAL););
    thc_done();

    return 0;
}
