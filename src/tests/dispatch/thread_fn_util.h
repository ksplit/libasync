#ifndef THREAD_FN_UTIL
#define THREAD_FN_UTIL

#include <libfipc.h>

#define ADD_2_FN 1
#define ADD_10_FN 2
#define TRANSACTIONS 3000
#define THD3_INTERVAL 61

int thread1_fn(void* data);
int thread2_fn(void* data);
int thread3_fn(void* data);

int send_and_get_response(
	struct thc_channel *chan,
	struct fipc_message *request,
	struct fipc_message **response,
    uint32_t msg_id);

#endif
