/*
 * awe-mapper.c
 *
 * This file is part of the asynchronous ipc for xcap.
 * This file is responsible for providing mappings
 * between an integer identifier and a pointer to
 * an awe struct.
 *
 * Author: Michael Quigley
 * Date: January 2016 
 */

#ifdef LCD_DOMAINS
#include <lcd_config/pre_hook.h>
#endif

#ifdef LINUX_KERNEL
#include <linux/bug.h>
#include <linux/string.h>
#include <linux/slab.h>
#define printf printk
#else
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#endif

#include <awe_mapper.h>

#ifdef LCD_DOMAINS
#include <lcd_config/post_hook.h>
#endif

#ifndef LINUX_KERNEL_MODULE
#undef EXPORT_SYMBOL
#define EXPORT_SYMBOL(x)
#endif

/* Linux kernel only provides ffs variant, which operates on 32-bit registers.
 * For promoting the bsf instruction to 64-bit, intel manual suggests to use
 * REX.W prefix to the instruction. However, when the operands are 64-bits, gcc
 * already promotes bsf to 64-bit.
 */
static __always_inline int ffsll(long long x)
{
	long long r;

	/*
	 * AMD64 says BSFL won't clobber the dest reg if x==0; Intel64 says the
	 * dest reg is undefined if x==0, but their CPU architect says its
	 * value is written to set it to the same as before, except that the
	 * top 32 bits will be cleared.
	 *
	 * We cannot do this on 32 bits because at the very least some
	 * 486 CPUs did not behave this way.
	 */
	asm("bsf %1,%0"
	    : "=r" (r)
	    : "rm" (x), "0" (-1));
	return r + 1;
}

/*
 * Initilaizes awe mapper.
 */
void
inline
LIBASYNC_FUNC_ATTR
awe_mapper_init(void)
{
#ifdef LINUX_KERNEL
    awe_table_t* awe_map;
    awe_map = kzalloc(sizeof(awe_table_t), GFP_KERNEL);
#else
    awe_table_t* awe_map = calloc(sizeof(awe_table_t), 1);
#endif

    if (!awe_map)
    {
        printf("No space left for awe_map_ptr\n");
        return;
    }
#ifdef LINUX_KERNEL
    awe_map->awe_bitmap = ~0UL;
#else
    awe_map->awe_bitmap = ~0LL;
#endif
    set_awe_map((awe_table_t*) awe_map);
    current->ptstate = PTS();
}



/*
 * Uninitilaizes awe mapper.
 */
void
inline
LIBASYNC_FUNC_ATTR
awe_mapper_uninit(void)
{
    awe_table_t *awe_map =  get_awe_map();
    if (awe_map) {
#ifdef LINUX_KERNEL
        kfree(awe_map);
#else
        free(awe_map);
#endif
        set_awe_map(NULL);
    }
}


/*
 * Returns new available id.
 */
int inline
_awe_mapper_create_id(awe_table_t *awe_map)
{
    int id;
#ifdef LINUX_KERNEL
    id = ffsll(awe_map->awe_bitmap);
#elif defined(linux)
    id = __builtin_ffsll(awe_map->awe_bitmap);
#endif
    awe_map->awe_bitmap &= ~(1LL << (id - 1));

    assert(id != 0);
#ifndef NDEBUG
    if (!id)
        printk("%s, id %d, bitmap %016llx\n", __func__, id, awe_map->awe_bitmap);
#endif
    return id; 
}

/*
 * Returns new available id.
 */
int inline
LIBASYNC_FUNC_ATTR 
awe_mapper_create_id(int *id)
{
    awe_table_t *awe_map =  get_awe_map();
    *id = _awe_mapper_create_id(awe_map);
    return 0;
}
EXPORT_SYMBOL(awe_mapper_create_id);


void 
LIBASYNC_FUNC_ATTR 
awe_mapper_uninit_with_pts(void *pts)
{
    PTState_t *ptstate = (PTState_t *)pts;
    awe_table_t *awe_map =  ptstate->awe_map;
    kfree(awe_map);
}
