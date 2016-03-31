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
	BUILD_BUG_ON(THC_RESERVED_MSG_FLAG_BITS > 32)

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
static inline uint32_t thc_set_msg_id(struct fipc_message *msg,
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

int thc_ipc_recv(struct fipc_ring_channel *chnl, 
                 unsigned long msg_id, 
                 struct fipc_message** out_msg);

int thc_poll_recv_group(struct thc_channel_group* chan_group, 
                        struct thc_channel_group_item** chan_group_item, 
                        struct fipc_message** out_msg);

int thc_poll_recv(struct thc_channel_group_item* item,
                 struct fipc_message** out_msg);

int thc_channel_group_init(struct thc_channel_group* channel_group);

int thc_channel_group_item_add(struct thc_channel_group* channel_group,
                          struct thc_channel_group_item* item);

void thc_channel_group_item_remove(struct thc_channel_group* channel_group, 
				struct thc_channel_group_item* item);

int thc_channel_group_item_get(struct thc_channel_group* channel_group, 
                               int index, 
                               struct thc_channel_group_item **out_item);
#endif
