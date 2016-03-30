#ifndef THC_IPC_H
#define THC_IPC_H

#include <libfipc.h>
#include <thc_ipc_types.h>

#define THC_MSG_TYPE(msg) ((msg)->regs[(FIPC_NR_REGS) - 2])
#define THC_MSG_ID(msg)   ((msg)->regs[(FIPC_NR_REGS) - 1])

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
