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
	volatile void ** frame = (volatile void**)__builtin_frame_address(0);
	volatile void *ret_addr = *(frame + 1);
    int recv_ct = 0;

    *(frame + 1) = NULL;
    //NOTE:max_recv_ct is just for testing
    DO_FINISH_(ipc_dispatch,{
        int curr_ind     = 0;
        int* curr_ind_pt = &curr_ind;
        struct thc_channel_group_item* curr_item;
        struct ipc_message* curr_msg;

        uint32_t do_finish_awe_id = awe_mapper_create_id();
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
                        printk(KERN_ERR "Channel %d function not allocated, message dropped\n", curr_ind_pt);
                    }
            }
        }
    });

	*(frame + 1) = ret_addr;

    return 0;
}
EXPORT_SYMBOL(thc_dispatch_loop_test);
