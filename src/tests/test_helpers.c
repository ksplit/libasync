#include "test_helpers.h"



static int test_fipc_compare(const void *_a, const void *_b){

	u64 a = *((u64 *)_a);
	u64 b = *((u64 *)_b);

	if(a < b)
		return -1;
	if(a > b)
		return 1;
	return 0;
}


void test_fipc_dump_time(unsigned long *time, unsigned long num_transactions)
{

	int i;
	unsigned long long counter = 0;
    unsigned long min;
	unsigned long max;

	for (i = 0; i < num_transactions; i++) {
		counter+= time[i];
	}

	sort(time, num_transactions, sizeof(unsigned long), test_fipc_compare, NULL);

	min = time[0];
	max = time[num_transactions-1];
	//counter = min;

	printk(KERN_ERR "MIN\tMAX\tAVG\tMEDIAN\n");
	printk(KERN_ERR "%lu & %lu & %llu & %lu \n", min, max, counter/num_transactions, time[num_transactions/2]);
}


