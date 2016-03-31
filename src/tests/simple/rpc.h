/*
 * rpc.h
 *
 * Internal defs
 *
 * Copyright: University of Utah
 */
#ifndef LIBFIPC_RPC_TEST_H
#define LIBFIPC_RPC_TEST_H

#include <libfipc.h>
#include <thc_ipc.h>

enum fn_type {
	NULL_INVOCATION, 
	ADD_CONSTANT, 
	ADD_NUMS, 
	ADD_3_NUMS,
	ADD_4_NUMS, 
	ADD_5_NUMS, 
	ADD_6_NUMS
};

/* must be divisible by 6... because I call 6 functions in the callee.c */
#define TRANSACTIONS 60

/* thread main functions */
int callee(void *_callee_channel_header);
int caller(void *_caller_channel_header);
#endif /* LIBFIPC_RPC_TEST_H */
