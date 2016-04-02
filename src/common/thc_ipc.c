
#ifdef LCD_DOMAINS
#include <lcd_config/pre_hook.h>
#endif

#include <thc_ipc.h>
#include <thc_ipc_types.h>
#include <libfipc_types.h>
#include <awe_mapper.h>

#ifdef LCD_DOMAINS
#include <lcd_config/post_hook.h>
#endif

#ifndef LINUX_KERNEL_MODULE
#undef EXPORT_SYMBOL
#define EXPORT_SYMBOL(x)
#endif

//assumes msg is a valid received message
static int thc_recv_predicate(struct fipc_message* msg, void* data)
{
    struct predicate_payload* payload_ptr = (struct predicate_payload*)data;

    if( thc_get_msg_type(msg) == (uint32_t)msg_type_request )
    {
        payload_ptr->msg_type = msg_type_request;
        return 1;
    }
    else if( thc_get_msg_id(msg) == payload_ptr->expected_msg_id )
    {
        payload_ptr->msg_type = msg_type_response;
        return 1; //message for this awe
    }
    else
    {
        payload_ptr->actual_msg_id = thc_get_msg_id(msg);
        return 0; //message not for this awe
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

int 
LIBASYNC_FUNC_ATTR 
thc_ipc_recv(struct fipc_ring_channel *chnl, 
	unsigned long msg_id, 
	struct fipc_message** out_msg)
{
    struct predicate_payload payload = {
        .expected_msg_id = msg_id
    };
    int ret;
    while( true )
    {
        ret = fipc_recv_msg_if(chnl, thc_recv_predicate, &payload, out_msg);
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
            THCYieldToIdAndSave((uint32_t)payload.actual_msg_id, (uint32_t) msg_id); 
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
EXPORT_SYMBOL(thc_ipc_recv);

int 
LIBASYNC_FUNC_ATTR 
thc_poll_recv_group(struct thc_channel_group* chan_group, 
		struct thc_channel_group_item** chan_group_item, 
		struct fipc_message** out_msg)
{
    struct thc_channel_group_item* curr_item;
    struct fipc_message* recv_msg;
    int ret;

    list_for_each_entry(curr_item, &(chan_group->head), list)
    {
        ret = thc_poll_recv(curr_item, &recv_msg);
        
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
thc_poll_recv(struct thc_channel_group_item* item,
	struct fipc_message** out_msg)
{
    struct predicate_payload payload;
    int ret;

    while( true )
    {
        ret = fipc_recv_msg_if(item->channel, poll_recv_predicate, &payload, out_msg);
        if( !ret ) //message for us
        {
            return 0; 
        }
        else if( ret == -ENOMSG ) //message not for us
        {
            THCYieldToId((uint32_t)payload.actual_msg_id); 
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
EXPORT_SYMBOL(thc_poll_recv);

int
LIBASYNC_FUNC_ATTR
thc_ipc_call(struct fipc_ring_channel *chnl,
	struct fipc_message *request,
	struct fipc_message **response)
{
    uint32_t msg_id;
    int ret;
    /*
     * Get an id for our current awe, and store in request.
     */
    msg_id = awe_mapper_create_id();
    thc_set_msg_type(request, msg_type_request);
    thc_set_msg_id(request, msg_id);
    /*
     * Send request
     */
    ret = fipc_send_msg_end(chnl, request);
    if (ret) {
        printk(KERN_ERR "thc: error sending request");
        goto fail1;	
    }
    /*
     * Receive response
     */
    ret = thc_ipc_recv(chnl, msg_id, response);
    if (ret) {
        printk(KERN_ERR "thc: error receiving response");
        goto fail1;
    }

    return 0;

fail1:
    awe_mapper_remove_id(msg_id);
    return ret;
}
EXPORT_SYMBOL(thc_ipc_call);

int
LIBASYNC_FUNC_ATTR
thc_ipc_send(struct fipc_ring_channel *chnl,
	struct fipc_message *request,
	uint32_t *request_cookie)
{
    uint32_t msg_id;
    int ret;
    /*
     * Get an id for our current awe, and store in request.
     */
    msg_id = awe_mapper_create_id();
    thc_set_msg_type(request, msg_type_request);
    thc_set_msg_id(request, msg_id);
    /*
     * Send request
     */
    ret = fipc_send_msg_end(chnl, request);
    if (ret) {
        printk(KERN_ERR "thc: error sending request");
        goto fail1;	
    }

    *request_cookie = msg_id;

    return 0;

fail1:
    awe_mapper_remove_id(msg_id);
    return ret;
}
EXPORT_SYMBOL(thc_ipc_send);

int
LIBASYNC_FUNC_ATTR
thc_ipc_reply(struct fipc_ring_channel *chnl,
	uint32_t request_cookie,
	struct fipc_message *response)
{
    thc_set_msg_type(response, msg_type_response);
    thc_set_msg_id(response, request_cookie);
    return fipc_send_msg_end(chnl, response);
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
