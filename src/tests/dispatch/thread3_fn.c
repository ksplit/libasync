#include <thc.h>
#include <thc_ipc.h>
#include <thc_ipc_types.h>
#include <libfipc.h>
#include <thcinternal.h>
#include "../test_helpers.h"
#include "thc_dispatch_test.h"
#include "thread_fn_util.h"
#include <awe_mapper.h>
#include <linux/delay.h>

static struct thc_channel_group* rx_group;



static int add_10_fn(struct thc_channel* chan, struct fipc_message* msg)
{
    unsigned long msg_id = thc_get_msg_id(msg);
    unsigned long reg0   = fipc_get_reg0(msg);
    unsigned long reg1   = fipc_get_reg1(msg);
    unsigned long result = reg0 + reg1 + 10;
	struct fipc_message* out_msg;
    fipc_recv_msg_end(thc_channel_to_fipc(chan),msg);
    msleep(10);
   
    if( test_fipc_blocking_send_start(thc_channel_to_fipc(chan), &out_msg) )
    {
         printk(KERN_ERR "Error getting send message for add_10_fn.\n");
    }
    fipc_set_reg0(out_msg, result);
    set_fn_type(out_msg, ADD_10_FN);
    
    if( thc_ipc_reply(chan, msg_id, out_msg) )
    {
        printk(KERN_ERR "Error sending message for add_10_fn.\n");
    }
    
    return 0;
}

static int thread2_dispatch_fn(struct thc_channel_group_item* item, struct fipc_message* msg)
{
    struct thc_channel* chan = item->channel;
    switch( get_fn_type(msg) )
    {
        case ADD_10_FN:
            return add_10_fn(chan, msg);
        default:
            printk(KERN_ERR "FN: %d is not a valid function type\n", get_fn_type(msg));
    }
    return 1;
}

int thread3_fn(void* group)
{
    struct thc_channel_group_item *thrd2_item;
    thc_init();
    rx_group = (struct thc_channel_group*)group;
    thc_channel_group_item_get(rx_group, 0, &thrd2_item);
    thrd2_item->dispatch_fn = thread2_dispatch_fn;
    LCD_MAIN(thc_dispatch_loop_test(rx_group, TRANSACTIONS / THD3_INTERVAL););
    thc_done();

    return 0;
}
