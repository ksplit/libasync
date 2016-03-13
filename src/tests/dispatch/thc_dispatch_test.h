#ifndef THC_DISPATCH_TEST_H
#define THC_DISPATCH_TEST_H 

#include <thc_ipc_types.h>

int thc_dispatch_loop_test(struct thc_channel_group* rx_group, int max_recv_ct);

static inline void ipc_dispatch_print_sp(char* time)
{
    unsigned long sp;
    asm volatile("mov %%rsp, %0" : "=g"(sp) : :);
    //printk(KERN_ERR "%s sp: %lx\n", time, sp);
}
#endif
