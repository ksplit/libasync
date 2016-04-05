#ifndef THC_IPC_TYPES_H
#define THC_IPC_TYPES_H

#ifdef LINUX_KERNEL

#include <libfipc.h>
#include <linux/list.h>

typedef enum {
    msg_type_unspecified,
    msg_type_request,
    msg_type_response,
} msg_type_t;

struct predicate_payload
{
    unsigned long expected_msg_id;
    unsigned long actual_msg_id;
    msg_type_t msg_type;
};

/*
 * struct thc_channel_group
 *
 * Represents a linked list of thc_channel_group_items.
 */
struct thc_channel_group
{
    struct list_head head;
    int size;
};

enum {
    THC_CHANNEL_GROUP_ITEM_LIVE,
    THC_CHANNEL_GROUP_ITEM_DEAD,
};

/*
 * struct thc_channel_group_item
 *
 * Contains a channel and a function that should get called when a message
 * is received on the channel.
 */
struct thc_channel_group_item
{
    int state;
    atomic_t refcnt;
    struct list_head list;
    struct fipc_ring_channel *channel;
    struct thc_channel_group *group;
    int (*dispatch_fn)(struct fipc_ring_channel*, struct fipc_message*);
};

static inline int 
thc_channel_group_item_is_live(struct thc_channel_group_item *item)
{
	return item->state == THC_CHANNEL_GROUP_ITEM_LIVE;
}

static inline int 
thc_channel_group_item_is_dead(struct thc_channel_group_item *item)
{
	return item->state == THC_CHANNEL_GROUP_ITEM_DEAD;
}

static inline void
thc_channel_group_item_mark_dead(struct thc_channel_group_item *item)
{
	item->state = THC_CHANNEL_GROUP_ITEM_DEAD;
}

#endif

#endif
