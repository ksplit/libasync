#include "thc_dispatch_test.h"
#include <thc.h>
#include <thcinternal.h>
#include <libfipc.h>
#include <awe_mapper.h>
#include <linux/types.h>
#include <thc_ipc.h>

#include "thread_fn_util.h"

//max_recv_ct just for testing
int thc_dispatch_loop_test(struct thc_channel_group* rx_group, int max_recv_ct)
{
    int recv_ct = 0;

    //NOTE:max_recv_ct is just for testing
    DO_FINISH_(ipc_dispatch,{
        int curr_ind     = 0;
        int* curr_ind_pt = &curr_ind;
        struct thc_channel_group_item* curr_item;
        struct fipc_message* curr_msg;

        while( recv_ct < max_recv_ct )
        {
           curr_ind = 0;
           if( !thc_poll_recv_group(rx_group, &curr_item, &curr_msg) )
            {
                recv_ct++;
                     
                    if( curr_item->dispatch_fn )
                    {
                        ASYNC_({
                            curr_item->dispatch_fn(curr_item->channel, curr_msg);
                        },ipc_dispatch);
                    }
                    else
                    {
                        printk(KERN_ERR "Channel %d function not allocated, message dropped\n", *curr_ind_pt);
                    }
            }
        }
    });

    return 0;
}
EXPORT_SYMBOL(thc_dispatch_loop_test);
