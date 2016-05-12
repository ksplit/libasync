/*
 * test_helpers.h
 *
 * Some common utilities for setting up threads, pinning them
 * to cores, setting up message buffers, etc.
 *
 * Copyright: University of Utah
 */
#ifndef FIPC_KERNEL_TEST_HELPERS_H
#define FIPC_KERNEL_TEST_HELPERS_H

#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/kernel.h>
#include <linux/sort.h>
#include <libfipc.h>
#include <thc_ipc.h>

#define LCD_MAIN(_CODE)    do {                        \
                                        \
            /* NULL out return address on stack so that libasync */ \
            /* will stop stack walk here.                 */ \
            /*                             */    \
            /* XXX: A touch of arch-dependent code here, but     */    \
            /* no biggie. When I used gcc's                 */    \
            /* __builtin_frame_address it broke (gcc null'd out  */    \
            /* the return address, but didn't restore it ... ?)  */    \
            /*                             */    \
            volatile void **__frame_ptr;                \
            volatile void *__saved_ret_addr;            \
            asm ("movq %%rbp, %0" : "=g"(__frame_ptr) ::);        \
            __saved_ret_addr = *(__frame_ptr + 1);            \
            *(__frame_ptr + 1) = NULL;                \
                                        \
            do { _CODE } while(0);                    \
                                        \
            /* Restore old return address to stack. */        \
            *(__frame_ptr + 1) = __saved_ret_addr;            \
                                        \
        } while (0);

int test_fipc_run_test_fn(int (*fn)(void), int core_num);

static inline
struct task_struct*
test_pin_to_core(void* thd_data, int (*threadfn)(void *data), int cpu_pin)
{
 	struct cpumask cpu_core;
	struct task_struct* thread = NULL;

        if (cpu_pin > num_online_cpus()) {
                pr_err("Trying to pin on cpu > than avail # cpus\n");
		goto fail1;
        }
	/*
	 * Create kernel thread
	 */
	thread = kthread_create(threadfn, thd_data, "Async.%d", cpu_pin);
	if (IS_ERR(thread)) {
		pr_err("Error while creating kernel thread\n");
		goto fail2;
	}
	/*
	 * Bump reference count, so even if thread dies before we have
	 * a chance to wait on it, we won't crash
	 */
	get_task_struct(thread);
	/*
	 * Assign thread to cpu_pin
	 */
	cpumask_clear(&cpu_core);
	cpumask_set_cpu(cpu_pin, &cpu_core);
	set_cpus_allowed_ptr(thread, &cpu_core);

	return thread;

fail2:
fail1:
	return thread;
}

static inline 
struct task_struct *
test_fipc_spawn_thread_with_channel(struct fipc_ring_channel *channel,
				int (*threadfn)(void *data),
				int cpu_pin)
{
   return test_pin_to_core(channel, threadfn, cpu_pin); 
}

static inline
void
test_fipc_release_thread(struct task_struct *thread)
{
	put_task_struct(thread);
}

static inline
int
test_fipc_wait_for_thread(struct task_struct *thread)
{
	int ret;
	ret = kthread_stop(thread);
	test_fipc_release_thread(thread);
	return ret;
}

static inline
int
test_fipc_create_channel(unsigned int buf_nr_pages_order, /* in pages */
			struct fipc_ring_channel **header_1, 
			struct fipc_ring_channel **header_2)
{
	int ret;
	void *buf1, *buf2;
	struct fipc_ring_channel *h1, *h2;
	unsigned int buf_order = buf_nr_pages_order + PAGE_SHIFT;
	/*
	 * Allocate buffer pages
	 */
	buf1 = (void *)__get_free_pages(GFP_KERNEL, buf_nr_pages_order);
	if (!buf1) {
		ret = -ENOMEM;
		goto fail1;
	}
	buf2 = (void *)__get_free_pages(GFP_KERNEL, buf_nr_pages_order);
	if (!buf2) {
		ret = -ENOMEM;
		goto fail2;
	}
	/*
	 * Initialize them
	 */
	ret = fipc_prep_buffers(buf_order, buf1, buf2);
	if (ret)
		goto fail3;
	/*
	 * Allocate and initialize headers
	 */
	h1 = kmalloc(sizeof(*h1), GFP_KERNEL);
	if (!h1) {
		ret = -ENOMEM;
		goto fail4;
	}
	h2 = kmalloc(sizeof(*h2), GFP_KERNEL);
	if (!h2) {
		ret = -ENOMEM;
		goto fail5;
	}
	ret = fipc_ring_channel_init(h1, buf_order, buf1, buf2);
	if (ret)
		goto fail6;
	ret = fipc_ring_channel_init(h2, buf_order, buf2, buf1);
	if (ret)
		goto fail7;
	
	*header_1 = h1;
	*header_2 = h2;

	return 0;

fail7:
fail6:
	kfree(h2);
fail5:
	kfree(h1);
fail4:
fail3:
	free_pages((unsigned long)buf1, buf_nr_pages_order);
fail2:
	free_pages((unsigned long)buf1, buf_nr_pages_order);
fail1:
	return ret;
}

static inline
void
test_fipc_free_channel(unsigned int buf_nr_pages_order, /* in pages */
		struct fipc_ring_channel *header_1, 
		struct fipc_ring_channel *header_2)
{
	/*
	 * Free buffers
	 */
	free_pages((unsigned long)header_1->tx.buffer, buf_nr_pages_order);
	free_pages((unsigned long)header_1->rx.buffer, buf_nr_pages_order);
	/*
	 * Free headers
	 */
	kfree(header_1);
	kfree(header_2);
}

static inline
int
test_fipc_blocking_recv_start(struct fipc_ring_channel *chnl, 
			struct fipc_message **out)
{
	int ret;
	for (;;) {
		/* Poll until we get a message or error */
		ret = fipc_recv_msg_start(chnl, out);
		if (!ret || ret != -EWOULDBLOCK)
			return ret;
		cpu_relax();
	}
}

static inline
int
test_fipc_blocking_send_start(struct fipc_ring_channel *chnl, 
			struct fipc_message **out)
{
	int ret;
	for (;;) {
		/* Poll until we get a free slot or error */
		ret = fipc_send_msg_start(chnl, out);
		if (!ret || ret != -EWOULDBLOCK)
			return ret;
		cpu_relax();
	}
}

static inline unsigned long test_fipc_start_stopwatch(void)
{
	unsigned long stamp;
	/*
	 * Assumes x86
	 *
	 * rdtsc returns current cycle counter on cpu; 
	 * low 32 bits in %rax, high 32 bits in %rdx.
	 *
	 * Note: We use rdtsc to start the stopwatch because it won't
	 * wait for prior instructions to complete (that we don't care
	 * about). It is not exact - meaning that instructions after
	 * it in program order may start executing before the read
	 * is completed (so we may slightly underestimate the time to
	 * execute the intervening instructions). But also note that
	 * the two subsequent move instructions are also counted against
	 * us (overestimate).
	 */
	asm volatile(
		"rdtsc\n\t"
		"shl $32, %%rdx\n\t"
		"or %%rdx, %%rax\n\t" 
		: "=a" (stamp)
		:
		: "rdx");
	return stamp;
}

static inline unsigned long test_fipc_stop_stopwatch(void)
{
	unsigned long stamp;
	/*
	 * Assumes x86
	 *
	 * Unlike start_stopwatch, we want to wait until all prior
	 * instructions are done, so we use rdtscp. (We don't care
	 * about the tsc aux value.)
	 */
	asm volatile(
		"rdtscp\n\t"
		"shl $32, %%rdx\n\t"
		"or %%rdx, %%rax\n\t" 
		: "=a" (stamp)
		:
		: "rdx", "rcx");
	return stamp;
}


void test_fipc_dump_time(unsigned long *time, unsigned long num_transactions);

static inline
int
get_fn_type(struct fipc_message *msg)
{
	return fipc_get_flags(msg) >> THC_RESERVED_MSG_FLAG_BITS;
}

static inline
void
set_fn_type(struct fipc_message *msg, uint32_t type)
{
	uint32_t flags = fipc_get_flags(msg);
	/* ensure type is in range */
	type &= (1 << (32 - THC_RESERVED_MSG_FLAG_BITS)) - 1;
	/* erase old type */
	flags &= ((1 << THC_RESERVED_MSG_FLAG_BITS) - 1);
	/* install new type */
	flags |= (type << THC_RESERVED_MSG_FLAG_BITS);
	fipc_set_flags(msg, flags);
}

#endif /* FIPC_KERNEL_TEST_HELPERS_H */
