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
 * struct thc_channel_group_item
 *
 * Contains a channel and a function that should get called when a message
 * is received on the channel.
 */
struct thc_channel_group_item
{
    struct list_head list;
    struct fipc_ring_channel * channel;
    int (*dispatch_fn)(struct fipc_ring_channel*, struct fipc_message*);
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

#endif

#endif
