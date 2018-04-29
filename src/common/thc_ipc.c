
#ifdef LCD_DOMAINS
#include <lcd_config/pre_hook.h>
#endif

#include <thc_ipc.h>
#include <thc_ipc_types.h>
#include <libfipc_types.h>
#include <awe_mapper.h>
#include <asm/atomic.h>
#include <linux/slab.h>

#ifdef LCD_DOMAINS
#include <lcd_config/post_hook.h>
#endif

#ifndef LINUX_KERNEL_MODULE
#undef EXPORT_SYMBOL
#define EXPORT_SYMBOL(x)
#endif

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

int
LIBASYNC_FUNC_ATTR
thc_channel_init_0(struct thc_channel *chnl,
		struct fipc_ring_channel *async_chnl)
{
    chnl->state = THC_CHANNEL_LIVE;
    atomic_set(&chnl->refcnt, 1);
    chnl->fipc_channel = async_chnl;
    chnl->one_slot = true;

    return 0;
}
EXPORT_SYMBOL(thc_channel_init_0);

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

//assumes msg is a valid received message
static int poll_recv_predicate(struct fipc_message* msg, void* data)
{
    struct predicate_payload* payload_ptr = (struct predicate_payload*)data;

    if( thc_get_msg_type(msg) == (uint32_t)msg_type_request )
    {
        payload_ptr->msg_type = msg_type_request;
        return 1;
    }
    else
    {
        payload_ptr->actual_msg_id = thc_get_msg_id(msg);
        return 0; //message not for this awe
    }
}

static void drop_one_rx_msg(struct thc_channel *chnl)
{
	int ret;
	struct fipc_message *msg;

	ret = fipc_recv_msg_start(thc_channel_to_fipc(chnl), &msg);
	if (ret)
		printk(KERN_ERR "thc_ipc_recv_response: failed to drop bad message\n");
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
		printk(KERN_ERR "thc_ipc_recv_response: Invalid request cookie 0x%x received; dropping the message\n",
			received_request_cookie);
		drop_one_rx_msg(chnl);
		return;
	}
	/*
	 * We were woken back up
	 */
	return;
}

inline int 
LIBASYNC_FUNC_ATTR 
thc_ipc_recv_response_ts(struct thc_channel *chnl, 
		uint32_t request_cookie, 
		struct fipc_message **response, uint64_t *ts)
{
	struct predicate_payload payload = {
		.expected_msg_id = request_cookie
	};
	int ret;
	uint64_t a;
retry:
	a = rdtsc();
        ret = fipc_recv_msg_if(thc_channel_to_fipc(chnl), thc_recv_predicate, 
			&payload, response);
	if (ret == 0) {
#if 0
               printk("%s, message for us!\n", __func__);
#endif
		/*
		 * Message for us; remove request_cookie from awe mapper
		 */
                awe_mapper_remove_id(request_cookie);
		//printk("%s, got msg for cookie %d\n", __func__, request_cookie);
	*ts = rdtsc() - a;
		return 0;
	} else if (ret == -ENOMSG && payload.msg_type == msg_type_request) {
#if 0
               printk("%s, request message?\n", __func__);
#endif
		/*
		 * Ignore requests; yield so someone else can receive it (msgs
		 * are received in fifo order).
		 */
		goto yield;
	} else if (ret == -ENOMSG && payload.msg_type == msg_type_response) {
#if 0
               printk("%s, for someone else?\n", __func__);
#endif
		/*
		 * Response for someone else. Try to yield to them.
		 */
		try_yield(chnl, request_cookie, payload.actual_msg_id);
		/*
		 * We either yielded to the pending awe the response
		 * belonged to, or the switch failed.
		 *
		 * Make sure the channel didn't die in case we did go to
		 * sleep.
		 */
		if (unlikely(thc_channel_is_dead(chnl)))
			return -EPIPE; /* someone killed the channel */
		goto retry;
	} else if (ret == -ENOMSG) {
#if 0
               printk("%s, unspecified message?\n", __func__);
#endif

		/*
		 * Unknown or unspecified message type; yield and let someone
		 * else handle it.
		 */
		goto yield;
	} else if (ret == -EWOULDBLOCK) {
#if 0
               printk("%s, no message in rx buffer, sleep?\n", __func__);
#endif
		/*
		 * No messages in rx buffer; go to sleep.
		 */
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
	//printk("%s, yielding to %d\n", __func__, request_cookie);
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
EXPORT_SYMBOL(thc_ipc_recv_response_ts);

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

retry:
        ret = fipc_recv_msg_if(thc_channel_to_fipc(chnl), thc_recv_predicate, 
			&payload, response);
	if (ret == 0) {
#if 0
               printk("%s, message for us!\n", __func__);
#endif
		/*
		 * Message for us; remove request_cookie from awe mapper
		 */
                awe_mapper_remove_id(request_cookie);
		//printk("%s, got msg for cookie %d\n", __func__, request_cookie);
		return 0;
	} else if (ret == -ENOMSG && payload.msg_type == msg_type_request) {
#if 0
               printk("%s, request message?\n", __func__);
#endif
		/*
		 * Ignore requests; yield so someone else can receive it (msgs
		 * are received in fifo order).
		 */
		goto yield;
	} else if (ret == -ENOMSG && payload.msg_type == msg_type_response) {
#if 0
               printk("%s, for someone else?\n", __func__);
#endif
		/*
		 * Response for someone else. Try to yield to them.
		 */
		try_yield(chnl, request_cookie, payload.actual_msg_id);
		/*
		 * We either yielded to the pending awe the response
		 * belonged to, or the switch failed.
		 *
		 * Make sure the channel didn't die in case we did go to
		 * sleep.
		 */
		if (unlikely(thc_channel_is_dead(chnl)))
			return -EPIPE; /* someone killed the channel */
		goto retry;
	} else if (ret == -ENOMSG) {
#if 0
               printk("%s, unspecified message?\n", __func__);
#endif

		/*
		 * Unknown or unspecified message type; yield and let someone
		 * else handle it.
		 */
		goto yield;
	} else if (ret == -EWOULDBLOCK) {
#if 0
               printk("%s, no message in rx buffer, sleep?\n", __func__);
#endif
		/*
		 * No messages in rx buffer; go to sleep.
		 */
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
	//printk("%s, yielding to %d\n", __func__, request_cookie);
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

#define FIPC_MSG_STATUS_AVAILABLE 0xdeaddeadUL
#define FIPC_MSG_STATUS_SENT      0xfeedfeedUL

static inline unsigned long inc_rx_slot(struct fipc_ring_channel *rc)
{
	return (rc->rx.slot++);
}

static inline unsigned long get_rx_idx(struct fipc_ring_channel *rc)
{
	return rc->rx.slot & rc->rx.order_two_mask;
}


static inline struct fipc_message* 
get_current_rx_slot(struct fipc_ring_channel *rc)
{
	return &rc->rx.buffer[get_rx_idx(rc)];
}

static inline int check_rx_slot_msg_waiting(struct fipc_message *slot)
{
	return slot->msg_status == FIPC_MSG_STATUS_SENT;
}

#define fipc_test_pause()    asm volatile ( "pause\n": : :"memory" );
//#define DEBUG_REQ_RESP
int
LIBASYNC_FUNC_ATTR
thc_ipc_recv_req_resp(struct thc_channel* channel,
		struct fipc_message ** out, int id,
		int (*sender_dispatch)(struct thc_channel*,
			struct fipc_message *, void *),
		void *ptr)

{
	int received_cookie;
	int got_resp = 0;
retry:
	while (1) {
		// Poll until we get a message or error
		*out = get_current_rx_slot(thc_channel_to_fipc(channel));

		if (!check_rx_slot_msg_waiting(*out)) {
			THCYieldAndSave(id);
			continue;
		}
		break;
	}

	if (likely(thc_get_msg_type(*out) == (uint32_t)msg_type_request)) {
		received_cookie = thc_get_msg_id(*out);
		if (received_cookie == id) {
#ifdef DEBUG_REQ_RESP
			printk("%s:%d got req for id %d (ctx = %d)\n",
				__func__, __LINE__, received_cookie, id);
#endif
			inc_rx_slot(thc_channel_to_fipc(channel));
			sender_dispatch(channel, *out, ptr);
			if (got_resp)
				return 0;
			else
				goto retry;
		} else {
#ifdef DEBUG_REQ_RESP
			printk("%s:%d yielding to id %d (ctx = %d)\n",
				__func__, __LINE__, received_cookie, id);
#endif
			THCYieldToIdAndSave(received_cookie, id);
		}
	} else if (likely(thc_get_msg_type(*out)
				== (uint32_t)msg_type_response)) {
		received_cookie = thc_get_msg_id(*out);
		if (received_cookie == id) {
#ifdef DEBUG_REQ_RESP
			printk("%s:%d got response for id %d (ctx = %d)\n",
				__func__, __LINE__, received_cookie, id);
#endif
			got_resp = 1;
			inc_rx_slot(thc_channel_to_fipc(channel));
			awe_mapper_remove_id(id);
			return 0;
		} else {
#ifdef DEBUG_REQ_RESP
			printk("%s:%d yielding to id %d (ctx = %d)\n",
				__func__, __LINE__, received_cookie, id);
#endif
			THCYieldToIdAndSave(received_cookie, id);
		}
	} else {
		printk("%s:%d msg not for us!\n", __func__, __LINE__);
		return 1; //message not for this awe
	}

	// We came back here but maybe we're the last AWE and 
        // we're re-started by do finish
	goto retry; 
	return 0;
}
EXPORT_SYMBOL(thc_ipc_recv_req_resp);

int
LIBASYNC_FUNC_ATTR
thc_ipc_recv_dispatch(struct thc_channel* channel, struct fipc_message ** out, int id,
		int (*sender_dispatch)(struct thc_channel*, struct fipc_message *, void *), void *ptr)
{
	int got_resp = 0;
	int received_cookie;
retry:
	while ( 1 )
	{
		// Poll until we get a message or error
		*out = get_current_rx_slot(thc_channel_to_fipc(channel));

		if (!check_rx_slot_msg_waiting(*out)) {
			//printf("No messages to recv, yield and save into id:%llu\n", id);
			THCYieldAndSave(id);
			continue;
		}

		break;
	}

	if (likely(thc_get_msg_type(*out) == (uint32_t)msg_type_request)) {
//		printf("%s:%d got request (ctx = %d)\n", __func__, __LINE__, id);
		inc_rx_slot(thc_channel_to_fipc(channel));
		sender_dispatch(channel, *out, ptr);
		if (got_resp)
			return 0;
		else
			goto retry;
	} else if (likely(thc_get_msg_type(*out) == (uint32_t)msg_type_response)) {

//		printf("%s:%d got response (ctx = %d)\n", __func__, __LINE__, id);

		received_cookie = thc_get_msg_id(*out);
		if (received_cookie == id) {
			got_resp = 1;
			inc_rx_slot(thc_channel_to_fipc(channel));
			awe_mapper_remove_id(id);
			return 0;
		} else {
//			printf("%s:%d yielding to id %d (ctx = %d)\n", __func__, __LINE__, received_cookie, id);
			THCYieldToIdAndSave(received_cookie, id);
		}
	} else {
		printk("%s:%d msg not for us!\n", __func__, __LINE__);
		return 1; //message not for this awe
	}

	// We came back here but maybe we're the last AWE and 
        // we're re-started by do finish
	goto retry; 
	return 0;
}
EXPORT_SYMBOL(thc_ipc_recv_dispatch);

int inline
LIBASYNC_FUNC_ATTR 
thc_ipc_recv_response_new(struct thc_channel* channel, uint32_t id,
		struct fipc_message** out)
{
	int ret;
	int received_cookie;
retry:
	while ( 1 )
	{
		// Poll until we get a message or error
		*out = get_current_rx_slot( thc_channel_to_fipc(channel));

		if ( ! check_rx_slot_msg_waiting( *out ) )
		{
			// No messages to receive, yield to next async
#ifndef BASE_CASE_NOASYNC
			THCYieldAndSave(id);
			fipc_test_pause();
#else
			fipc_test_pause();
#endif
			continue;
		}

		break;
	}

#ifndef BASE_CASE_NOASYNC
	received_cookie = thc_get_msg_id(*out);
	if (received_cookie == id) {
#endif
		inc_rx_slot( thc_channel_to_fipc(channel) );
		return 0;
#ifndef BASE_CASE_NOASYNC
	}
#endif
	
#ifndef BASE_CASE_NOASYNC
	ret = THCYieldToIdAndSave(received_cookie, id);
	 
	if (ret) {
		printk("ALERT: wrong id\n");
		return ret;
	}
#endif
	// We came back here but maybe we're the last AWE and 
        // we're re-started by do finish
	fipc_test_pause();
	goto retry; 
	return 0;
}
EXPORT_SYMBOL(thc_ipc_recv_response_new);

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
thc_poll_recv_group_2(struct thc_channel_group* chan_group,
		struct thc_channel_group_item** chan_group_item,
		struct fipc_message** out_msg)
{
    struct thc_channel_group_item *curr_item;
    //struct thc_channel_group_item *temp;
    struct fipc_message* recv_msg;
    int ret;

    //list_for_each_entry_safe(curr_item, temp, &(chan_group->head), list)
    list_for_each_entry(curr_item, &(chan_group->head), list)
    {
	if (curr_item->xmit_channel) {
		ret = fipc_nonblocking_recv_start_if(
			thc_channel_to_fipc(
			thc_channel_group_item_channel(curr_item)),
                        &recv_msg);
	} else {
	        ret = thc_ipc_poll_recv(thc_channel_group_item_channel(curr_item),
                        &recv_msg);
	}

        if( !ret )
        {
            *chan_group_item = curr_item;
            *out_msg         = recv_msg;
            return 0;
        }
    }

    return -EWOULDBLOCK;
}
EXPORT_SYMBOL(thc_poll_recv_group_2);

int
LIBASYNC_FUNC_ATTR
thc_ipc_poll_recv_2(struct thc_channel* chnl,
	struct fipc_message** out_msg)
{
    int ret;
    uint32_t received_cookie;

    while (true)
    {
        ret = fipc_recv_msg_poll(
			thc_channel_to_fipc(chnl),
			out_msg, &received_cookie);
        if( !ret ) //message for us
        {
            return 0; 
        }
        else if( ret == -ENOMSG ) //message not for us
        {
            THCYieldToId((uint32_t)received_cookie);
            if (unlikely(thc_channel_is_dead(chnl)))
                return -EPIPE; // channel died
        }
        else if( ret == -EWOULDBLOCK ) //no message, return
        {
            return ret;
        }
        else
        {
            printk(KERN_ERR "error in thc_poll_recv: %d\n", ret);
            return ret;
        }
    }
}
EXPORT_SYMBOL(thc_ipc_poll_recv_2);

int
LIBASYNC_FUNC_ATTR
thc_ipc_poll_recv(struct thc_channel* chnl,
	struct fipc_message** out_msg)
{
    struct predicate_payload payload;
    int ret;

    while( true )
    {
	if (chnl->one_slot)
	        ret = fipc_recv_msg_if_0(thc_channel_to_fipc(chnl), poll_recv_predicate,
                       &payload, out_msg);
	else
	        ret = fipc_recv_msg_if(thc_channel_to_fipc(chnl), poll_recv_predicate,
                       &payload, out_msg);
        if( !ret ) //message for us
        {
            return 0; 
        }
        else if( ret == -ENOMSG ) //message not for us
        {
         //printk("%s: Yield to %d\n", __func__, (uint32_t)payload.actual_msg_id);

            THCYieldToId((uint32_t)payload.actual_msg_id);
            if (unlikely(thc_channel_is_dead(chnl)))
                return -EPIPE; // channel died
        }
        else if( ret == -EWOULDBLOCK ) //no message, return
        {
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
thc_ipc_call(struct thc_channel *chnl,
	struct fipc_message *request,
	struct fipc_message **response)
{
    uint32_t request_cookie;
    int ret;
    /*
     * Send request
     */
    ret = thc_ipc_send_request(chnl, request, &request_cookie);
    if (ret) {
        printk(KERN_ERR "thc_ipc_call: error sending request\n");
        goto fail1;
    }
//#ifdef IPC_DEBUG
    //printk("%s, request_cookie %d\n", __func__, request_cookie);
//#endif
    /*
     * Receive response
     */
    ret = thc_ipc_recv_response(chnl, request_cookie, response);
    if (ret) {
        printk(KERN_ERR "thc_ipc_call: error receiving response\n");
        goto fail2;
    }

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
    ret = fipc_send_msg_end(thc_channel_to_fipc(chnl), request);
    if (ret) {
        printk(KERN_ERR "thc: error sending request");
        goto fail1;	
    }

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
    item->xmit_channel = false;
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
