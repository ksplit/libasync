#include <thc_ipc.h>
#include <libfipc_types.h>
#include <awe-mapper.h>

//assumes msg is a valid received message
static int recv_predicate(struct fipc_message* msg, void* data)
{
    struct predicate_payload* payload_ptr = (struct predicate_payload*)data;

    if( THC_MSG_TYPE(msg) == (uint32_t)msg_type_request )
    {
        payload_ptr->msg_type = msg_type_request;
        return 1;
    }
    else if( THC_MSG_ID(msg) == payload_ptr->expected_msg_id )
    {
        payload_ptr->msg_type = msg_type_response;
        return 1; //message for this awe
    }
    else
    {
        return 0; //message not for this awe
    }
}


int thc_ipc_recv(struct fipc_ring_channel *chnl, unsigned long msg_id, struct fipc_message** out_msg)
{
    struct predicate_payload payload = {
        .expected_msg_id = msg_id
    };
    int ret;
    while( true )
    {
        ret = fipc_recv_msg_if(chnl, recv_predicate, &payload, out_msg);
        if( !ret ) //message for us
        {
            if( payload.msg_type == msg_type_response )
            {
                awe_mapper_remove_id(msg_id);
            }
            return 0; 
        }
        else if( ret == -ENOMSG ) //message not for us
        {
            THCYieldToId((uint32_t)THC_MSG_ID(*out_msg), (uint32_t) msg_id); 
        }
        else if( ret == -EWOULDBLOCK ) //no message, Yield
        {
            THCYieldAndSave((uint32_t) msg_id); 
        }
        else
        {
            printk(KERN_ERR "error in thc_ipc_recv: %d\n", ret);
            return ret;
        }
    }
}
