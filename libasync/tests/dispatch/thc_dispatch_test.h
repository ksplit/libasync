#ifndef IPC_DISPATCH_H
#define IPC_DISPATCH_H

int ipc_dispatch_loop(struct ttd_ring_channel_group* rx_group, int max_recv_ct);

static inline void ipc_dispatch_print_sp(char* time)
{
    unsigned long sp;
    asm volatile("mov %%rsp, %0" : "=g"(sp) : :);
    printk(KERN_ERR "%s sp: %lx\n", time, sp);
}
#endif
