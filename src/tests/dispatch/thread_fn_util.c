#include <thc_ipc.h>
#include "thread_fn_util.h"


int send_and_get_response(
	struct thc_channel *chan,
	struct fipc_message *request,
	struct fipc_message **response,
    uint32_t msg_id)
{
	int ret;
	struct fipc_message *resp;

	/*
	 * Mark the request as sent
	 */
	ret = fipc_send_msg_end(thc_channel_to_fipc(chan), request);
	if (ret) {
		pr_err("failed to mark request as sent, ret = %d\n", ret);
		goto fail1;
	}
	/*
	 * Try to get the response
	 */
    ret = thc_ipc_recv_response(chan, msg_id, &resp);
	if (ret) {
		pr_err("failed to get a response, ret = %d\n", ret);
		goto fail2;
	}
	*response = resp;

	return 0;

fail2:
fail1:
	return ret;
}
