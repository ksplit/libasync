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

static inline
int
get_fn_type(struct fipc_message *msg)
{
	return fipc_get_flags(msg);
}

static inline
void
set_fn_type(struct fipc_message *msg, uint32_t type)
{
	fipc_set_flags(msg, type);
}

#endif /* LIBFIPC_RPC_TEST_H */
