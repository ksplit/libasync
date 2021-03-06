/*
 * caller.c
 *
 * The "caller side" of the channel
 *
 * Copyright: University of Utah
 */

#include <linux/random.h>
#include "rpc.h"
#include <thc_ipc.h>
#include <thc.h>
#include <thcinternal.h>
#include <awe_mapper.h>
#include "../test_helpers.h"

static inline int finish_response_check_fn_type(struct fipc_ring_channel *chnl,
						struct fipc_message *response,
						enum fn_type expected_type)
{
	int ret;
	enum fn_type actual_type = get_fn_type(response);
	ret = fipc_recv_msg_end(chnl, response);
	if (ret) {
		pr_err("Error finishing receipt of response, ret = %d\n", ret);
		return ret;
	} else if (actual_type != expected_type) {
		pr_err("Unexpected fn type: actual = %d, expected = %d\n",
			actual_type, expected_type);
		return -EINVAL;
	} else {
		return 0;
	}
}

static inline int finish_response_check_fn_type_and_reg0(
	struct fipc_ring_channel *chnl,
	struct fipc_message *response,
	enum fn_type expected_type,
	unsigned long expected_reg0)
{
	int ret;
	enum fn_type actual_type = get_fn_type(response);
	unsigned long actual_reg0 = fipc_get_reg0(response);

	ret = fipc_recv_msg_end(chnl, response);
	if (ret) {
		pr_err("Error finishing receipt of response, ret = %d\n", ret);
		return ret;
	} else if (actual_type != expected_type) {
		pr_err("Unexpected fn type: actual = %d, expected = %d\n",
			actual_type, expected_type);
		return -EINVAL;
	} else if (actual_reg0 != expected_reg0) {
		pr_err("Unexpected return value (reg0): actual = 0x%lx, expected = 0x%lx\n",
			actual_reg0, expected_reg0);
		return -EINVAL;

	} else {
		return 0;
	}
}

static int noinline __used
async_add_nums(struct thc_channel *chan, unsigned long trans, 
	unsigned long res1)
{
	struct fipc_message *request;
	struct fipc_message *response;
	int ret;
	/*
	 * Set up request
	 */
	ret = test_fipc_blocking_send_start(thc_channel_to_fipc(chan), 
					&request);
	if (ret) {
		pr_err("Error getting send message, ret = %d\n", ret);
		goto fail;
	}
        /*
	 * Set up rpc ms
         */
	set_fn_type(request, ADD_NUMS);
	fipc_set_reg0(request, trans);
	fipc_set_reg1(request, res1);
	/*
	 * Send request, and get response
	 */
	ret = thc_ipc_call(chan, request, &response);
	if (ret) {
		pr_err("Error getting response, ret = %d\n", ret);
		goto fail;
	}
	/*
	 * Maybe check message
	 */
	return finish_response_check_fn_type_and_reg0(
		thc_channel_to_fipc(chan),
		response, 
		ADD_NUMS,
		trans + res1);

fail:
	return ret;
}

int __caller(void *_caller_channel_header)
{
	struct fipc_ring_channel *fchan = _caller_channel_header;
	struct thc_channel *chan;
	unsigned long transaction_id;

	chan = kmalloc(sizeof(*chan), GFP_KERNEL);
	if (!chan) {
		pr_err("thc channel alloc failed\n");
		return -ENOMEM;
	}

	thc_channel_init(chan, fchan);

	thc_init();
	/*
	 * Add nums
	 */
	DO_FINISH(

		for (transaction_id = 0; 
		     transaction_id < TRANSACTIONS;
		     transaction_id++) {

			ASYNC(

				int ret;
				void *tid = __builtin_alloca(
					sizeof(transaction_id));
				memcpy(tid, (void *)&transaction_id,
					sizeof(transaction_id));

				ret = async_add_nums(chan, 
						*(unsigned long *)tid,
						1000);
				if (ret)
					pr_err("error doing add nums, ret = %d\n", ret);

				);

		}

		);

	pr_err("Complete\n");

	kfree(chan);

	thc_done();

	return 0;
}

int caller(void *_caller_channel_header)
{
	int result;

	LCD_MAIN({ result = __caller(_caller_channel_header); });

	return result;
}
