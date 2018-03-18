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


#endif 
