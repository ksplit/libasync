
#ifdef LCD_DOMAINS
#include <lcd_config/pre_hook.h>
#endif

#include <liblcd/console.h>
#include <thc_ipc.h>
#include <thc_ipc_types.h>
#include <libfipc_types.h>
#include <awe_mapper.h>
#include <asm/atomic.h>
#include <linux/slab.h>
#include <benchmark.h>

#ifdef LCD_DOMAINS
#include <lcd_config/post_hook.h>
#endif

#ifndef LINUX_KERNEL_MODULE
#undef EXPORT_SYMBOL
#define EXPORT_SYMBOL(x)
#endif

INIT_BENCHMARK_DATA_LCD(thc_yield);
/* static inline void __ipc_debug(const char* format, ...)
{
	va_list args;

    va_start(args, format);
    lcd_printk(format, args);
    va_end(args);
}
*/

#define IPC_DEBUG(format,...) lcd_printk("%p %s: %d: " format "\n",current,__func__,__LINE__,##__VA_ARGS__)

void LIBASYNC_FUNC_ATTR thc_pts_set_chnl(struct thc_channel *ch)
{
	if(PTS()) {
		PTS()->chnl = ch;
	}
}
EXPORT_SYMBOL(thc_pts_set_chnl);

struct thc_channel* LIBASYNC_FUNC_ATTR thc_pts_get_chnl(void)
{
	if(PTS()) {
		return PTS()->chnl;
	}
	else {
		return NULL;
	}
}
EXPORT_SYMBOL(thc_pts_get_chnl);

void LIBASYNC_FUNC_ATTR thc_pts_set_state(bool state)
{
	if(PTS()) {
		PTS()->thread_ctx = state;
	}
}

EXPORT_SYMBOL(thc_pts_set_state);

bool LIBASYNC_FUNC_ATTR thc_pts_get_state(void)
{
	if(PTS()) {
		return PTS()->thread_ctx;
	}
	else {
		return false;
	}
}
EXPORT_SYMBOL(thc_pts_get_state);

int 
LIBASYNC_FUNC_ATTR
thc_channel_init(struct thc_channel *chnl, 
		struct fipc_ring_channel *async_chnl)
{
    chnl->state = THC_CHANNEL_LIVE;
    atomic_set(&chnl->refcnt, 1);
    chnl->fipc_channel = async_chnl;

    return 0;
}
EXPORT_SYMBOL(thc_channel_init);

static int thc_recv_predicate(struct fipc_message* msg, void* data)
{
	struct predicate_payload* payload_ptr = (struct predicate_payload*)data;
	payload_ptr->msg_type = thc_get_msg_type(msg);

	if (payload_ptr->msg_type == msg_type_request) {
		/*
		 * Ignore requests
		 */
		return 0;
	} else if (payload_ptr->msg_type == msg_type_response) {

		payload_ptr->actual_msg_id = thc_get_msg_id(msg);

		if (payload_ptr->actual_msg_id == 
			payload_ptr->expected_msg_id) {
			/*
			 * Response is for us; tell libfipc we want it
			 */
			return 1;
		} else {
			/*
			 * Response for another awe; we will switch to
			 * them. Meanwhile, the message should stay in
			 * rx. They (the awe we switch to) will pick it up.
			 */
			return 0;
		}
	} else {
		/*
		 * Ignore any other message types
		 */
		return 0;
	}
}

static inline
int
async_msg_get_fn_type(struct fipc_message *msg)
{
        return fipc_get_flags(msg) >> THC_RESERVED_MSG_FLAG_BITS;
}

/*
//assumes msg is a valid received message
static int poll_recv_predicate_lcd(struct fipc_message* msg, void* data)
{
    struct predicate_payload* payload_ptr = (struct predicate_payload*)data;

    if( thc_get_msg_type(msg) == (uint32_t)msg_type_request )
    {
        payload_ptr->msg_type = msg_type_request;
        return 1;
    }
    else if ( thc_get_msg_type(msg) == (uint32_t)msg_type_response )
    {
        payload_ptr->actual_msg_id = thc_get_msg_id(msg);
	payload_ptr->fn_type = async_msg_get_fn_type(msg); 
        return 0; //message not for this awe
    }
    else {
	    printk(KERN_ERR "poll_recv_predicate: Unexpected msg type 0x%x \n",thc_get_msg_type(msg));
     	return 0;
    }
}*/


//assumes msg is a valid received message
static int poll_recv_predicate(struct fipc_message* msg, void* data)
{
    struct predicate_payload* payload_ptr = (struct predicate_payload*)data;

    if( thc_get_msg_type(msg) == (uint32_t)msg_type_request )
    {
        payload_ptr->msg_type = msg_type_request;
        return 1;
    }
    else if ( thc_get_msg_type(msg) == (uint32_t)msg_type_response )
    {
        payload_ptr->actual_msg_id = thc_get_msg_id(msg);
        return 0; //message not for this awe
    }
    else {
	    printk(KERN_ERR "poll_recv_predicate: Unexpected msg type 0x%x \n",thc_get_msg_type(msg));
     	return 0;
    }
}

static void drop_one_rx_msg(struct thc_channel *chnl)
{
	int ret;
	struct fipc_message *msg;

	ret = fipc_recv_msg_start(thc_channel_to_fipc(chnl), &msg);
	if (ret)
		printk(KERN_ERR "thc_ipc_recv_response: failed to drop bad message current: %p ptstate %p \n", current, current->ptstate);
	ret = fipc_recv_msg_end(thc_channel_to_fipc(chnl), msg);
	if (ret)
		printk(KERN_ERR "thc_ipc_recv_response: failed to drop bad message (mark as received)\n");
	return;
}

static void try_yield(struct thc_channel *chnl, uint32_t our_request_cookie,
		uint32_t received_request_cookie)
{
	int ret;
	/*
	 * Switch to the pending awe the response belongs to
	 */
	ret = THCYieldToIdAndSave(received_request_cookie,
				our_request_cookie);
	if (ret) {
		/*
		 * Oops, the switch failed
		 */
		printk(KERN_ERR "thc_ipc_recv_response: Invalid request cookie 0x%x received; dropping the message: current %p ptstate %p\n",
			received_request_cookie, current, current->ptstate);
		drop_one_rx_msg(chnl);
		return;
	}
	/*
	 * We were woken back up
	 */
	return;
}

/* A variant that uses monitor/mwait to reduce cache bouncing */
int 
LIBASYNC_FUNC_ATTR 
thc_ipc_recv_resp_noyield_mwait(struct thc_channel *chnl, 
		struct fipc_message **response)
{
	int ret;

	ret = fipc_recv_msg_mwait(thc_channel_to_fipc(chnl), response);
	if (ret == 0) {
		/*
		 * Message for us; remove request_cookie from awe mapper
		 */
		return 0;
	} 
	else {
		/*
		 * Error
		 */
		printk(KERN_ERR "thc_ipc_recv_response: fipc returned %d\n", 
			ret);
		return ret;
	}

}
EXPORT_SYMBOL(thc_ipc_recv_resp_noyield_mwait);
#define fipc_test_pause()    asm volatile ( "pause\n": : :"memory" );
int 
LIBASYNC_FUNC_ATTR 
thc_ipc_recv_resp_noyield(struct thc_channel *chnl, 
		struct fipc_message **response)
{
	int ret;

retry:
	ret = fipc_recv_msg_start(thc_channel_to_fipc(chnl), response);
	if (ret == 0) {
		/*
		 * Message for us; remove request_cookie from awe mapper
		 */
		return 0;
	} else if (ret == -EWOULDBLOCK) {
		/*
		 * No messages in rx buffer; go to sleep.
		 */
		//cpu_relax();
		fipc_test_pause();
		goto retry;
	} else {
		/*
		 * Error
		 */
		printk(KERN_ERR "thc_ipc_recv_response: fipc returned %d\n", 
			ret);
		return ret;
	}

	/*
	 * We were woken up; make sure the channel didn't die while
	 * we were asleep.
	 */
	if (unlikely(thc_channel_is_dead(chnl)))
                return -EPIPE; /* someone killed the channel */
	else
		goto retry;
}
EXPORT_SYMBOL(thc_ipc_recv_resp_noyield);

int 
LIBASYNC_FUNC_ATTR 
thc_ipc_recv_response_lcd(struct thc_channel *chnl, 
		uint32_t request_cookie, 
		struct fipc_message **response)
{
	struct predicate_payload payload = {
		.expected_msg_id = request_cookie
	};
	int ret;

	//IPC_DEBUG("req cookie 0x%x", request_cookie);

    	//printk("thc_ipc: call recv_msg_if \n");
retry:
        ret = fipc_recv_msg_if(thc_channel_to_fipc(chnl), thc_recv_predicate, 
			&payload, response);
	if (ret == 0) {
		/*
		 * Message for us; remove request_cookie from awe mapper
		 */
		//IPC_DEBUG("rxd message response for us, request_cookie 0x%x",
		//request_cookie);
                awe_mapper_remove_id(request_cookie);
		return 0;
	} else if (ret == -ENOMSG && payload.msg_type == msg_type_request) {
		/*
		 * Ignore requests; yield so someone else can receive it (msgs
		 * are received in fifo order).
		 */
		//IPC_DEBUG("rxd request for dispatch loop, we are waiting for "
		//"request_cookie 0x%x "
		//,request_cookie);
		goto yield;
	} else if (ret == -ENOMSG && payload.msg_type == msg_type_response) {
		/*
		 * Response for someone else. Try to yield to them.
		 */
		//IPC_DEBUG("response for someone else, we are waiting for "
		//	"request_cookie 0x%x resp is for req_cookie 0x%x"
		//	,request_cookie, payload.actual_msg_id);
		try_yield(chnl, request_cookie, payload.actual_msg_id);
		
		//IPC_DEBUG("somebody woke us up");
		/*
		 * We either yielded to the pending awe the response
		 * belonged to, or the switch failed.
		 *
		 * Make sure the channel didn't die in case we did go to
		 * sleep.
		 */
		if (unlikely(thc_channel_is_dead(chnl))) {
    			printk("channel dead \n");
			return -EPIPE; /* someone killed the channel */
		}
		goto retry;
	} else if (ret == -ENOMSG) {
		/*
		 * Unknown or unspecified message type; yield and let someone
		 * else handle it.
		 */
		//IPC_DEBUG("unknown message type");
    		//printk("thc_ipc: unknown msg \n");
		goto yield;
	} else if (ret == -EWOULDBLOCK) {
		/*
		 * No messages in rx buffer; go to sleep.
		 */
    		//printk("thc_ipc: no msg \n");
	  //		IPC_DEBUG("no messages in the rx buffer");
		goto yield;
	} else {
		/*
		 * Error
		 */
		printk(KERN_ERR "thc_ipc_recv_response: fipc returned %d\n", 
			ret);
		return ret;
	}

yield:
	/*
	 * Go to sleep, we will be woken up at some later time
	 * by the dispatch loop or some other awe.
	 */
	//BENCH_BEGIN_LCD(thc_yield);
	THCYieldAndSave(request_cookie);
	//BENCH_END_LCD(thc_yield);
	/*
	 * We were woken up; make sure the channel didn't die while
	 * we were asleep.
	 */
	if (unlikely(thc_channel_is_dead(chnl)))
                return -EPIPE; /* someone killed the channel */
	else
		goto retry;
}
EXPORT_SYMBOL(thc_ipc_recv_response_lcd);


void
LIBASYNC_FUNC_ATTR 
thc_ipc_dump_stat(void) {
	BENCH_COMPUTE_STAT(thc_yield);
} 
EXPORT_SYMBOL(thc_ipc_dump_stat);

int 
LIBASYNC_FUNC_ATTR 
thc_ipc_recv_response(struct thc_channel *chnl, 
		uint32_t request_cookie, 
		struct fipc_message **response)
{
	struct predicate_payload payload = {
		.expected_msg_id = request_cookie
	};
	int ret;

	//IPC_DEBUG("req cookie 0x%x", request_cookie);

    	//printk("thc_ipc: call recv_msg_if \n");
retry:
        ret = fipc_recv_msg_if(thc_channel_to_fipc(chnl), thc_recv_predicate, 
			&payload, response);
	if (ret == 0) {
		/*
		 * Message for us; remove request_cookie from awe mapper
		 */
		//IPC_DEBUG("rxd message response for us, request_cookie 0x%x",
		//request_cookie);
                awe_mapper_remove_id(request_cookie);
		return 0;
	} else if (ret == -ENOMSG && payload.msg_type == msg_type_request) {
		/*
		 * Ignore requests; yield so someone else can receive it (msgs
		 * are received in fifo order).
		 */
		//IPC_DEBUG("rxd request for dispatch loop, we are waiting for "
		//"request_cookie 0x%x "
		//,request_cookie);
		goto yield;
	} else if (ret == -ENOMSG && payload.msg_type == msg_type_response) {
		/*
		 * Response for someone else. Try to yield to them.
		 */
		//IPC_DEBUG("response for someone else, we are waiting for "
		//	"request_cookie 0x%x resp is for req_cookie 0x%x"
		//	,request_cookie, payload.actual_msg_id);
		try_yield(chnl, request_cookie, payload.actual_msg_id);
		
		//IPC_DEBUG("somebody woke us up");
		/*
		 * We either yielded to the pending awe the response
		 * belonged to, or the switch failed.
		 *
		 * Make sure the channel didn't die in case we did go to
		 * sleep.
		 */
		if (unlikely(thc_channel_is_dead(chnl))) {
    			printk("channel dead \n");
			return -EPIPE; /* someone killed the channel */
		}
		goto retry;
	} else if (ret == -ENOMSG) {
		/*
		 * Unknown or unspecified message type; yield and let someone
		 * else handle it.
		 */
		//IPC_DEBUG("unknown message type");
    		//printk("thc_ipc: unknown msg \n");
		goto yield;
	} else if (ret == -EWOULDBLOCK) {
		/*
		 * No messages in rx buffer; go to sleep.
		 */
    		//printk("thc_ipc: no msg \n");
	  //		IPC_DEBUG("no messages in the rx buffer");
		goto yield;
	} else {
		/*
		 * Error
		 */
		printk(KERN_ERR "thc_ipc_recv_response: fipc returned %d\n", 
			ret);
		return ret;
	}

yield:
	/*
	 * Go to sleep, we will be woken up at some later time
	 * by the dispatch loop or some other awe.
	 */
	THCYieldAndSave(request_cookie);
	/*
	 * We were woken up; make sure the channel didn't die while
	 * we were asleep.
	 */
	if (unlikely(thc_channel_is_dead(chnl)))
                return -EPIPE; /* someone killed the channel */
	else
		goto retry;
}
EXPORT_SYMBOL(thc_ipc_recv_response);

int 
LIBASYNC_FUNC_ATTR 
thc_poll_recv_group_klcd(struct thc_channel_group* chan_group, 
		struct thc_channel_group_item** chan_group_item, 
		struct fipc_message** out_msg)
{
    struct thc_channel_group_item *curr_item;
    struct fipc_message* recv_msg;
    int ret;

    list_for_each_entry(curr_item, &(chan_group->head), list)
    {
	ret = thc_ipc_poll_recv_klcd(thc_channel_group_item_channel(curr_item), 
                        &recv_msg);
        if( !ret )
        {
            *chan_group_item = curr_item;
            *out_msg         = recv_msg;
            
            return 0;
        }
    }

    return -EWOULDBLOCK;
}
EXPORT_SYMBOL(thc_poll_recv_group_klcd);


int 
LIBASYNC_FUNC_ATTR 
thc_poll_recv_group_lcd(struct thc_channel_group* chan_group, 
		struct thc_channel_group_item** chan_group_item, 
		struct fipc_message** out_msg)
{
    struct thc_channel_group_item *curr_item;
    struct fipc_message* recv_msg;
    int ret;

    list_for_each_entry(curr_item, &(chan_group->head), list)
    {
	ret = thc_ipc_poll_recv_lcd(thc_channel_group_item_channel(curr_item), 
                        &recv_msg);
        if( !ret )
        {
            *chan_group_item = curr_item;
            *out_msg         = recv_msg;
            
            return 0;
        }
    }

    return -EWOULDBLOCK;
}
EXPORT_SYMBOL(thc_poll_recv_group_lcd);


int 
LIBASYNC_FUNC_ATTR 
thc_poll_recv_group(struct thc_channel_group* chan_group, 
		struct thc_channel_group_item** chan_group_item, 
		struct fipc_message** out_msg)
{
    struct thc_channel_group_item *curr_item;
    struct fipc_message* recv_msg;
    int ret;

    list_for_each_entry(curr_item, &(chan_group->head), list)
    {
	ret = thc_ipc_poll_recv(thc_channel_group_item_channel(curr_item), 
                        &recv_msg);
        if( !ret )
        {
            *chan_group_item = curr_item;
            *out_msg         = recv_msg;
            
            return 0;
        }
    }

    return -EWOULDBLOCK;
}
EXPORT_SYMBOL(thc_poll_recv_group);

int 
LIBASYNC_FUNC_ATTR 
thc_ipc_poll_recv_klcd(struct thc_channel* chnl,
	struct fipc_message** out_msg)
{
    struct predicate_payload payload;
    int ret;

    while( true )
    {	
      //      	IPC_DEBUG("poll, top of loop");
        ret = fipc_recv_msg_klcd_if(thc_channel_to_fipc(chnl), poll_recv_predicate, 
                       &payload, out_msg);
        if( !ret ) //message for us
        {
		//IPC_DEBUG("got a request from other side, msg status 0x%x flags 0x%x",
		//	(*out_msg)->msg_status, (*out_msg)->flags);
            return 0; 
        }
        else if( ret == -ENOMSG ) //message not for us
        {
            //AB: Lets not yield for now, keep spinning until the
	    //thread that wants the message can take it
	    THCYieldToId((uint32_t)payload.actual_msg_id);  
	      		//IPC_DEBUG("yield returns -EINVAL %x",payload.actual_msg_id);
	    
	    //cpu_relax();
            if (unlikely(thc_channel_is_dead(chnl)))
                return -EPIPE; // channel died
        }
        else if( ret == -EWOULDBLOCK ) //no message, return
        {
	  //	IPC_DEBUG("nothing to rx in poll_recv");
            return ret;
        }
        else
        {
            printk(KERN_ERR "error in thc_poll_recv: %d\n", ret);
            return ret;
        }
    }
}
EXPORT_SYMBOL(thc_ipc_poll_recv_klcd);

/*
static __always_inline void bench_start(void) { 
	BENCH_BEGIN_LCD(thc_yield);
}
static __always_inline void bench_end(void) { 
	BENCH_END_LCD(thc_yield);
} */ 

int 
LIBASYNC_FUNC_ATTR 
thc_ipc_poll_recv_lcd(struct thc_channel* chnl,
	struct fipc_message** out_msg)
{
    struct predicate_payload payload;
    int ret;

    while( true )
    {	
      //      	IPC_DEBUG("poll, top of loop");
        ret = fipc_recv_msg_if(thc_channel_to_fipc(chnl), poll_recv_predicate, 
                       &payload, out_msg);
        if( !ret ) //message for us
        {
		//IPC_DEBUG("got a request from other side, msg status 0x%x flags 0x%x",
		//	(*out_msg)->msg_status, (*out_msg)->flags);
            return 0; 
        }
        else if( ret == -ENOMSG ) //message not for us
        {
	    //(payload.fn_type == 3) ? bench_start() : -1;
	    //(payload.fn_type == 3) ? count++ : -1;
	    THCYieldToId((uint32_t)payload.actual_msg_id); 
	    //(payload.fn_type == 3) ? bench_end() : -1;
	    //cpu_relax();
            if (unlikely(thc_channel_is_dead(chnl)))
                return -EPIPE; // channel died
        }
        else if( ret == -EWOULDBLOCK ) //no message, return
        {
	  //	IPC_DEBUG("nothing to rx in poll_recv");
            return ret;
        }
        else
        {
            printk(KERN_ERR "error in thc_poll_recv: %d\n", ret);
            return ret;
        }
    }
}
EXPORT_SYMBOL(thc_ipc_poll_recv_lcd);


int 
LIBASYNC_FUNC_ATTR 
thc_ipc_poll_recv(struct thc_channel* chnl,
	struct fipc_message** out_msg)
{
    struct predicate_payload payload;
    int ret;

    while( true )
    {	
      //      	IPC_DEBUG("poll, top of loop");
        ret = fipc_recv_msg_if(thc_channel_to_fipc(chnl), poll_recv_predicate, 
                       &payload, out_msg);
        if( !ret ) //message for us
        {
		//IPC_DEBUG("got a request from other side, msg status 0x%x flags 0x%x",
		//	(*out_msg)->msg_status, (*out_msg)->flags);
            return 0; 
        }
        else if( ret == -ENOMSG ) //message not for us
        {
	/* IPC_DEBUG("got a response from other side, msg status 0x%x flags 0x%x", */
	/* 		(*out_msg)->msg_status, (*out_msg)->flags); */
            THCYieldToId((uint32_t)payload.actual_msg_id); 
	      		//IPC_DEBUG("yield returns -EINVAL %x",payload.actual_msg_id);
	    
	    //	    	IPC_DEBUG("back in poll_recv");
            if (unlikely(thc_channel_is_dead(chnl)))
                return -EPIPE; // channel died
        }
        else if( ret == -EWOULDBLOCK ) //no message, return
        {
	  //	IPC_DEBUG("nothing to rx in poll_recv");
            return ret;
        }
        else
        {
            printk(KERN_ERR "error in thc_poll_recv: %d\n", ret);
            return ret;
        }
    }
}
EXPORT_SYMBOL(thc_ipc_poll_recv);


int
LIBASYNC_FUNC_ATTR
thc_ipc_call_noyield_single_chnl(struct thc_channel *chnl,
	struct fipc_message *request,
	struct fipc_message **response)
{
    int ret;
    /*
     * Send request
     */
    thc_set_msg_type(request, msg_type_request);
    fipc_send_msg_end(thc_channel_to_fipc(chnl), request);

    /*
     * Receive response
     */
    ret = thc_ipc_recv_resp_noyield(chnl, response);
    //ret = thc_ipc_recv_resp_noyield_mwait(chnl, response);
    if (ret) {
        printk(KERN_ERR "thc_ipc_call: error receiving response\n");
        goto fail2;
    }

    return 0;

fail2:
    return ret;
}
EXPORT_SYMBOL(thc_ipc_call_noyield_single_chnl);

int
LIBASYNC_FUNC_ATTR
thc_ipc_call_lcd(struct thc_channel *chnl,
	struct fipc_message *request,
	struct fipc_message **response)
{
    uint32_t request_cookie;
    int ret;
    /*
     * Send request
     */
    //if(async_msg_get_fn_type(request) == 15) {
//	    printk("thc_ipc: send req \n");
  //  }
	//IPC_DEBUG("start of call, sending request");
    ret = thc_ipc_send_request(chnl, request, &request_cookie);
    if (ret) {
        printk(KERN_ERR "thc_ipc_call: error sending request\n");
        goto fail1;
    }
    /*
     * Receive response
     */
    //if(async_msg_get_fn_type(request) == 15) {
//	    printk("thc_ipc: rx resp \n");
  //  }
	//IPC_DEBUG("receiving response, request cookie %d", request_cookie);	
    ret = thc_ipc_recv_response_lcd(chnl, request_cookie, response);
    if (ret) {
        printk(KERN_ERR "thc_ipc_call: error receiving response\n");
        goto fail2;
    }
	//IPC_DEBUG("got response");

    return 0;

fail2:
    awe_mapper_remove_id(request_cookie);
fail1:
    return ret;
}
EXPORT_SYMBOL(thc_ipc_call_lcd);


int
LIBASYNC_FUNC_ATTR
thc_ipc_call(struct thc_channel *chnl,
	struct fipc_message *request,
	struct fipc_message **response)
{
    uint32_t request_cookie;
    int ret;
    /*
     * Send request
     */
    //if(async_msg_get_fn_type(request) == 15) {
//	    printk("thc_ipc: send req \n");
  //  }
	//IPC_DEBUG("start of call, sending request");
    ret = thc_ipc_send_request(chnl, request, &request_cookie);
    if (ret) {
        printk(KERN_ERR "thc_ipc_call: error sending request\n");
        goto fail1;
    }
    /*
     * Receive response
     */
    //if(async_msg_get_fn_type(request) == 15) {
//	    printk("thc_ipc: rx resp \n");
  //  }
	//IPC_DEBUG("receiving response, request cookie %d", request_cookie);	
    ret = thc_ipc_recv_response(chnl, request_cookie, response);
    if (ret) {
        printk(KERN_ERR "thc_ipc_call: error receiving response\n");
        goto fail2;
    }
	//IPC_DEBUG("got response");

    return 0;

fail2:
    awe_mapper_remove_id(request_cookie);
fail1:
    return ret;
}
EXPORT_SYMBOL(thc_ipc_call);

int
LIBASYNC_FUNC_ATTR
thc_ipc_send_request(struct thc_channel *chnl,
		struct fipc_message *request,
		uint32_t *request_cookie)
{
    uint32_t msg_id;
    int ret;
    /*
     * Get an id for our current awe, and store in request.
     */
    ret = awe_mapper_create_id(&msg_id);
    if (ret) {
        printk(KERN_ERR "thc_ipc_send: error getting request cookie\n");
	goto fail0;
    }
    thc_set_msg_type(request, msg_type_request);
    thc_set_msg_id(request, msg_id);
    /*
     * Send request
     */
	//IPC_DEBUG("sending request with msg id 0x%x", msg_id);
    ret = fipc_send_msg_end(thc_channel_to_fipc(chnl), request);
    if (ret) {
        printk(KERN_ERR "thc: error sending request");
        goto fail1;	
    }
	//IPC_DEBUG("sent request");
    *request_cookie = msg_id;

    return 0;

fail1:
    awe_mapper_remove_id(msg_id);
fail0:
    return ret;
}
EXPORT_SYMBOL(thc_ipc_send_request);

int
LIBASYNC_FUNC_ATTR
thc_ipc_reply(struct thc_channel *chnl,
	uint32_t request_cookie,
	struct fipc_message *response)
{
	//IPC_DEBUG("replying with req cookie %d", request_cookie);
    thc_set_msg_type(response, msg_type_response);
    thc_set_msg_id(response, request_cookie);
    return fipc_send_msg_end(thc_channel_to_fipc(chnl), response);
}
EXPORT_SYMBOL(thc_ipc_reply);

int 
LIBASYNC_FUNC_ATTR 
thc_channel_group_init(struct thc_channel_group* channel_group)
{
    INIT_LIST_HEAD(&(channel_group->head));
    channel_group->size = 0;

    return 0;
}
EXPORT_SYMBOL(thc_channel_group_init);

int
LIBASYNC_FUNC_ATTR
thc_channel_group_item_init(struct thc_channel_group_item *item,
			struct thc_channel *chnl,
			int (*dispatch_fn)(struct thc_channel_group_item*, 
					struct fipc_message*))
{
    INIT_LIST_HEAD(&item->list);
    item->channel = chnl;
    item->dispatch_fn = dispatch_fn;

    return 0;
}
EXPORT_SYMBOL(thc_channel_group_item_init);

int 
LIBASYNC_FUNC_ATTR 
thc_channel_group_item_add(struct thc_channel_group* channel_group, 
                          struct thc_channel_group_item* item)
{
    list_add_tail(&(item->list), &(channel_group->head));
    channel_group->size++;

    return 0;
}
EXPORT_SYMBOL(thc_channel_group_item_add);

void
LIBASYNC_FUNC_ATTR 
thc_channel_group_item_remove(struct thc_channel_group* channel_group, 
			struct thc_channel_group_item* item)
{
    list_del_init(&(item->list));
    channel_group->size--;
}
EXPORT_SYMBOL(thc_channel_group_item_remove);

int 
LIBASYNC_FUNC_ATTR 
thc_channel_group_item_get(struct thc_channel_group* channel_group, 
                               int index, 
                               struct thc_channel_group_item **out_item)
{

    int curr_index = 0;
    list_for_each_entry((*out_item), &(channel_group->head), list)
    {
        if( curr_index == index )
        {
            return 0;
        }
        curr_index++;
    }

    return 1;
}
EXPORT_SYMBOL(thc_channel_group_item_get);
