#ifndef THC_IPC_H
#define THC_IPC_H

#include <libfipc.h>


#define THC_MSG_TYPE(msg) ((msg)->flags)
#define THC_MSG_ID(msg)   ((msg)->regs[(FIPC_NR_REGS) - 1])

int thc_ipc_recv(struct fipc_ring_channel *chnl, unsigned long msg_id, struct fipc_message** out_msg);

typedef enum {
    msg_type_unspecified,
    msg_type_request,
    msg_type_response,
} msg_type_t;

static struct predicate_payload
{
    unsigned long expected_msg_id;
    msg_type_t msg_type;
};



#endif
