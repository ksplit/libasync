#ifndef THC_IPC_H
#define THC_IPC_H

#include <libfipc.h>
#include <thc_ipc_types.h>
#include <awe_mapper.h>

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
/*
 * thc_ipc_recv
 * 
 * Performs an async receive of a message.
 * msg_id is used to receive a message with a particular id and must always
 * be valid. If a message is received but its id does not match the id provided
 * to this function, then this function will check if the message corresponds
 * to another pending AWE, and if so, it will yield to that AWE. If there is 
 * not a message that has been received yet, it will yield to dispatch any
 * pending work. If a message is received that corresponds to the provided
 * msg_id, this function will put the message in the *out_msg parameter and return.
 * This last case is the only time the function will return assuming there are
 * no error conditions. In the other two cases involving yields, execution will
 * eventually come back to this function until it receives the message
 * corresponding to its msg_id. 
 *  It is the caller's responsibility to mark the message as having completed
 * the receive. (ex: fipc_recv_msg_end(chnl, msg))
 *
 * Returns 0 on success, non-zero otherwise.
 */
int thc_ipc_recv(struct fipc_ring_channel *chnl, 
                 unsigned long msg_id, 
                 struct fipc_message** out_msg);

/*
 * thc_ipc_call
 * 
 * Performs both a send and an async receive. 
 * request is the message to send.
 * (*response) will have the received message.
 *
 * It is the caller's responsibility to mark the message as having completed
 * the receive. (ex: fipc_recv_msg_end(chnl, msg))
 *
 * Returns 0 on success, non-zero otherwise.
 */
int thc_ipc_call(struct fipc_ring_channel *chnl, 
		struct fipc_message *request, 
		struct fipc_message **response);

/*
 * thc_ipc_send
 *
 * Associates a "request cookie" (the message id)
 * with the request and does the send (this is 
 * essentially the top half of thc_ipc_call).
 *
 * The request cookie is returned as an out param.
 *
 * The caller should invoke a subsequent thc_ipc_recv
 * with the request cookie as the msg_id argument to
 * get the response.
 *
 * Returns 0 on success, non-zero otherwise.
 */
int thc_ipc_send(struct fipc_ring_channel *chnl, 
		struct fipc_message *request, 
		uint32_t *request_cookie);

/*
 * thc_ipc_reply
 *
 * Sets the msg_id of response to the msg_id of request
 * and marks the response message as having a response message type.
 * Sends response message on the provided ring channel. 
 *
 * Returns 0 on success, non-zero otherwise.
 */
int thc_ipc_reply(struct fipc_ring_channel *chnl,
		uint32_t request_cookie,
		struct fipc_message *response);

/* thc_poll_recv
 *
 * Same as thc_ipc_recv except that if no message is present, it
 * returns -EWOULDBLOCK instead of yielding.
 * Additionally, the channel inside the thc_channel_group_item is what
 * is used instead of a channel directly.
 * There is also no msg_id passed in because poll_recv should not expect a
 * specific message.
 *
 * NOTE: The caller should hold a reference to item before calling this
 * function (i.e., the caller must ensure the ref count doesn't go to
 * zero while this function is in progress). Otherwise, the item could
 * be freed from under us while this function is in progress.
 *
 * Returns 0 on success, non-zero otherwise.
 */
int thc_poll_recv(struct thc_channel_group_item* item,
                 struct fipc_message** out_msg);

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
 * NOTE: The caller should "own" a reference to each of the channel group
 * items in the group (i.e., should ensure the reference count for every
 * channel in the group cannot suddenly drop to zero while this function
 * is in progress).
 *
 * Returns 0 on success, non-zero otherwise.
 */
int thc_poll_recv_group(struct thc_channel_group* chan_group, 
                        struct thc_channel_group_item** chan_group_item, 
                        struct fipc_message** out_msg);
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
				struct fipc_ring_channel *chnl,
				int (*dispatch_fn)(struct fipc_ring_channel*, 
						struct fipc_message*));

/*
 * thc_channel_group_item_inc_ref
 *
 * Bump the reference count on the channel group item.
 */
void thc_channel_group_item_inc_ref(struct thc_channel_group_item *item);

/*
 * thc_channel_group_item_dec_ref
 *
 * Decrement the reference count on the channel group item. If the 
 * reference count reaches zero, the item is removed from the
 * channel group it belongs to (if any), and it is freed. Returns
 * non-zero if the ref count reaches zero (zero otherwise).
 *
 * NOTE: Assumes item was allocated with kmalloc.
 */
int thc_channel_group_item_dec_ref(struct thc_channel_group_item *item);

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
 * thc_channel_group_item_add
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
#endif
