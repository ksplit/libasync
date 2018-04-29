#ifndef THC_IPC_H
#define THC_IPC_H

#include <libfipc.h>
#include <thc_ipc_types.h>
#include <awe_mapper.h>
#include <asm/atomic.h>

/* THC MESSAGES ------------------------------------------------------------ */

#define THC_RESERVED_MSG_FLAG_BITS (2 + AWE_TABLE_ORDER)

/* Message type is in low 2 bits of flags */
static inline uint32_t thc_get_msg_type(struct fipc_message *msg)
{
	/* Caught at compile time */
	BUILD_BUG_ON(THC_RESERVED_MSG_FLAG_BITS > 32);

	return fipc_get_flags(msg) & 0x3;
}
static inline void thc_set_msg_type(struct fipc_message *msg, uint32_t type)
{
	fipc_set_flags(msg,
		/* erase old type, and mask off low 2 bits of type */
		(fipc_get_flags(msg) & ~0x3) | (type & 0x3));
}
/* Message id is in bits 2..(2 + AWE_TABLE_ORDER) bits */
static inline uint32_t thc_get_msg_id(struct fipc_message *msg)
{
	/* shift off type bits, and mask off msg id */
	return (fipc_get_flags(msg) >> 2) & ((1 << AWE_TABLE_ORDER) - 1);
}
static inline void thc_set_msg_id(struct fipc_message *msg,
				uint32_t msg_id)
{
	uint32_t flags = fipc_get_flags(msg);
	/* erase old msg id, if any */
	flags &= ~(((1 << AWE_TABLE_ORDER) - 1) << 2);
	/* mask off relevant bits of msg id (to ensure it is in range),
	 * and install in flags. */
	flags |= (msg_id & ((1 << AWE_TABLE_ORDER) - 1)) << 2;
	fipc_set_flags(msg, flags);
}

static inline uint32_t thc_get_request_cookie(struct fipc_message *request)
{
    return thc_get_msg_id(request);
}

static inline void thc_kill_request_cookie(uint32_t request_cookie)
{
    awe_mapper_remove_id(request_cookie);
}

/* THC CHANNELS ------------------------------------------------------------ */

/*
 * thc_channel_init
 *
 * Initialize a thc channel struct with the given fipc channel.
 *
 * The channel item's reference count is initialized to 1, and
 * state to LIVE.
 *
 * Returns non-zero on failure.
 */
int thc_channel_init(struct thc_channel *chnl, 
		struct fipc_ring_channel *async_chnl);

int thc_channel_init_0(struct thc_channel *chnl,
		struct fipc_ring_channel *async_chnl);
/*
 * thc_channel_inc_ref
 *
 * Bump the reference count on the channel.
 */
static inline void thc_channel_inc_ref(struct thc_channel *chnl)
{
    atomic_inc(&chnl->refcnt);
}

/*
 * thc_channel_group_item_dec_ref
 *
 * Decrement the reference count on the channel.
 *
 * Returns non-zero if the ref count reached zero.
 */
static inline int thc_channel_dec_ref(struct thc_channel *chnl)
{
    return atomic_dec_and_test(&chnl->refcnt);
}

static inline int thc_channel_is_live(struct thc_channel *chnl)
{
    return chnl->state == THC_CHANNEL_LIVE;
}

static inline int thc_channel_is_dead(struct thc_channel *chnl)
{
    return chnl->state == THC_CHANNEL_DEAD;
}

static inline void thc_channel_mark_dead(struct thc_channel *chnl)
{
    chnl->state = THC_CHANNEL_DEAD;
}

static inline struct fipc_ring_channel *
thc_channel_to_fipc(struct thc_channel *chnl)
{
    return chnl->fipc_channel;
}

/* IPC FUNCTIONS ----------------------------------------------------------- */

/* 
 * NOTE: The caller of these functions should ensure they hold a reference
 * to the thc_channel (i.e., the caller is the creator of the channel, or
 * has explicitly taken a reference via thc_channel_inc_ref). This is
 * especially important in thc_ipc_recv_response/thc_ipc_poll_recv, where 
 * the "state of the world" can change when we yield and then get scheduled 
 * again later on.
 */

/*
 * thc_ipc_send_request
 *
 * Use case: You are expecting a response after
 * this send, and you want the calling awe to handle
 * the response. (If the send can be immediately
 * followed by the receive, use thc_ipc_call
 * instead.)
 *
 * Associates a "request cookie" (the message id)
 * with the request and does the send (this is 
 * essentially the top half of thc_ipc_call).
 *
 * The request cookie is returned as an out param.
 *
 * The caller should invoke a subsequent thc_ipc_recv
 * with the request cookie in order to get the response.
 *
 * (If you don't expect a response that targets the
 * calling awe, you should just do a regular fipc
 * send instead.)
 *
 * If you know the other side of the fipc channel
 * won't be sending a response and you want to
 * delete the request_cookie, you should call
 * thc_kill_request_cookie to make it available
 * for future use again.
 *
 * Returns 0 on success, non-zero otherwise.
 */
int thc_ipc_send_request(struct thc_channel *chnl, 
			struct fipc_message *request, 
			uint32_t *request_cookie);

/*
 * thc_ipc_recv_response
 * 
 * Receive the response from a prior thc_ipc_send_request.
 *
 * thc_ipc_recv_response will peek at the fipc channel to see
 * if the response has arrived. It uses the request_cookie to
 * identify the response.
 *
 * There are a few things that can happen:
 *
 *    1 - No message is in the channel (i.e., the rx buffer).
 *        thc_ipc_recv_response will yield back to the dispatch
 *        loop, to be scheduled at a later time. (If there are
 *        no other awe's, the dispatch loop will just keep
 *        switching back to thc_ipc_recv_response, and thc_ipc_recv_response
 *        keep yielding.)
 *    2 - A response arrives from the other side, and its
 *        request cookie matches request_cookie. thc_ipc_recv_response
 *        will return the response as an out parameter. This
 *        is the target scenario.
 *    3 - A response arrives with a request cookie that does not match
 *        request_cookie. There are two subcases here:
 *
 *          3a - The response has a request_cookie that
 *               belongs to a pending awe. thc_ipc_recv_response
 *               will switch to that pending awe X. If X
 *               had not called thc_ipc_recv_response when
 *               it yielded, or does not end up calling
 *               thc_ipc_recv_response when it is woken up,
 *               you could run into trouble.
 *
 *         3b - The response has a request_cookie that
 *              *does not* belong to a pending awe. 
 *              thc_ipc_recv_response will print an error notice
 *              to the console, receive and drop the message,
 *              and start over.
 *
 *        XXX: This case may pose a security issue if the sender of the
 *        response passes a request cookie for an awe that didn't
 *        even send a request ...
 *
 *    4 - A *request* arrives from the other side, rather than
 *        a response. (This can happen if you are using the
 *        same fipc channel for requests and responses, i.e.,
 *        you receive requests and responses in the same rx
 *        buffer.) thc_ipc_recv_response will ignore the message, and
 *        yield as if no message was received. (Some other
 *        awe will need to pick up the request.)
 * 
 * It is the caller's responsibility to mark the message as having completed
 * the receive. (ex: fipc_recv_msg_end(chnl, msg))
 *
 * Returns 0 on success, non-zero otherwise.
 */
int thc_ipc_recv_response(struct thc_channel *chnl, 
			uint32_t request_cookie, 
			struct fipc_message **response);

int thc_ipc_recv_response_ts(struct thc_channel *chnl, 
			uint32_t request_cookie, 
			struct fipc_message **response, uint64_t *ts);

int thc_ipc_recv_response_new(struct thc_channel *chnl, 
			uint32_t request_cookie, 
			struct fipc_message **response);

int thc_ipc_recv_dispatch(struct thc_channel* channel,
			struct fipc_message ** out,
			int id,
			int (*sender_dispatch)(struct thc_channel*, struct fipc_message *, void *),
			void *);
int thc_ipc_recv_req_resp(struct thc_channel* channel,
		struct fipc_message ** out, int id,
		int (*sender_dispatch)(struct thc_channel*,
			struct fipc_message *, void *),
		void *ptr);

/* thc_poll_recv
 *
 * Looks at the fipc channel, and does the following:
 *
 *    -- If a request is waiting in the rx buffer, returns
 *       it.
 *    -- If a response is waiting, attempts to dispatch to
 *       the matching pending awe (like thc_ipc_recv_response does).
 *       If no awe matches, prints error to console, drops
 *       message, and returns -ENOMSG.
 *    -- If no message is waiting in the rx buffer, returns
 *       -EWOULDBLOCK (rather than yieldling).
 *
 * There is no request_cookie passed in because thc_ipc_poll_recv should 
 * not expect a specific message.
 *
 * Returns 0 on success, non-zero otherwise.
 */
int thc_ipc_poll_recv(struct thc_channel *chnl,
		struct fipc_message **out_msg);

int thc_ipc_poll_recv_2(struct thc_channel *chnl,
		struct fipc_message **out_msg);
/*
 * thc_ipc_call
 * 
 * Performs a thc_ipc_send_request followed by an immediate
 * thc_ipc_recv_response.
 *
 * It is the caller's responsibility to mark the message as having completed
 * the receive. (ex: fipc_recv_msg_end(chnl, msg))
 *
 * Returns 0 on success, non-zero otherwise.
 */
int thc_ipc_call(struct thc_channel *chnl, 
		struct fipc_message *request, 
		struct fipc_message **response);

/*
 * thc_ipc_reply
 *
 * Send a response with request cookie request_cookie
 * on the channel's tx buffer.
 *
 * This is for the receiver of a request. The receiver does
 * some work, and then wants to send a response.
 *
 * You can get the request_cookie in a request message via
 * thc_get_request_cookie.
 *
 * Sets the request_cookie of response and marks the response 
 * message as having a response message type. Sends response 
 * message on the provided channel. 
 *
 * Returns 0 on success, non-zero otherwise.
 */
int thc_ipc_reply(struct thc_channel *chnl,
		uint32_t request_cookie,
		struct fipc_message *response);

/* CHANNEL GROUPS ---------------------------------------------------------- */

/*
 * thc_channel_group_init
 *
 * Initializes thc_channel_group structure after it has been allocated.
 *
 * Returns 0 on success, non-zero otherwise.
 */
int thc_channel_group_init(struct thc_channel_group* channel_group);

/*
 * thc_channel_group_item_init
 *
 * Initializes a channel group item with the provided ring channel and
 * dispatch function.
 *
 * The channel item's reference count is initialized to 1.
 *
 * Returns 0 on success, non-zero otherwise.
 */
int thc_channel_group_item_init(struct thc_channel_group_item *item,
                                struct thc_channel *chnl,
                                int (*dispatch_fn)(struct thc_channel_group_item*, 
                                struct fipc_message*));

/*
 * thc_channel_group_item_add
 *
 * Adds a thc_channel_group_item to a thc_channel_group.
 *
 * Returns 0 on success, non-zero otherwise.
 */
int thc_channel_group_item_add(struct thc_channel_group* channel_group,
                          struct thc_channel_group_item* item);


/*
 * thc_channel_group_item_remove
 *
 * Removes a thc_channel_group_item from a thc_channel_group.
 */
void thc_channel_group_item_remove(struct thc_channel_group* channel_group, 
				struct thc_channel_group_item* item);

/*
 * thc_channel_group_item_get
 *
 * Gets a thc_channel_group_item at a particular index in a channel_group.
 * Puts the thc_channel_group_item in the out parameter (*out_item) on success.
 *
 * Returns 0 on success. Returns 1 if index is out of range.
 */
int thc_channel_group_item_get(struct thc_channel_group* channel_group, 
                               int index, 
                               struct thc_channel_group_item **out_item);

static inline struct thc_channel *
thc_channel_group_item_channel(struct thc_channel_group_item *item)
{
    return item->channel;
}

static inline struct fipc_ring_channel *
thc_channel_group_item_to_fipc(struct thc_channel_group_item *item)
{
    return thc_channel_group_item_channel(item)->fipc_channel;
}

/*
 * thc_poll_recv_group
 *
 * Calls thc_poll_recv on a list of thc_channel_group_items represented
 * by thc_channel_group. If there is a message available that does not
 * correspond to a pending AWE, the thc_channel_group_item that corresponds
 * to the received message is set to the out param (*chan_group_item),
 * and the received message is set to the out param (*out_msg).
 * If there is no message available in any of the thc_channel_group_items,
 * then this function returns -EWOULDBLOCK.
 *
 * NOTE: The caller should "own" a reference to each of the thc channels
 * in the group (i.e., should ensure the reference count for every
 * channel in the group cannot suddenly drop to zero while this function
 * is in progress).
 *
 * Returns 0 on success, non-zero otherwise.
 */
int thc_poll_recv_group(struct thc_channel_group* chan_group, 
                        struct thc_channel_group_item** chan_group_item, 
                        struct fipc_message** out_msg);

int thc_poll_recv_group_2(struct thc_channel_group* chan_group, 
                        struct thc_channel_group_item** chan_group_item, 
                        struct fipc_message** out_msg);

#endif
