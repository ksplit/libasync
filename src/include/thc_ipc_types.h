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
    uint32_t expected_msg_id;
    uint32_t actual_msg_id;
    msg_type_t msg_type;
    int fn_type;
};


enum {
    THC_CHANNEL_LIVE,
    THC_CHANNEL_DEAD,
};

/*
 * struct thc_channel
 *
 * Wraps around a libfipc channel. One of the motivations is we need to
 * do reference counting due to interesting computation patterns with
 * awe's that share async channels.
 */
struct thc_channel 
{
    int state;
    atomic_t refcnt;
    struct fipc_ring_channel *fipc_channel;
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
    struct thc_channel *channel;
    int channel_id;
    int (*dispatch_fn)(struct thc_channel_group_item*, struct fipc_message*);
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
