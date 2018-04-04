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


/*
 * Initilaizes awe mapper.
 */
void 
LIBASYNC_FUNC_ATTR 
awe_mapper_init(void)
{
#ifdef LINUX_KERNEL
    awe_table_t* awe_map = kzalloc(sizeof(awe_table_t), GFP_KERNEL);
#else
    awe_table_t* awe_map = calloc(sizeof(awe_table_t), 1);
#endif

    if( !awe_map )
    {
        printf("No space left for awe_map_ptr\n");         
        return;
    }

    awe_map->awe_bitmap |= ~0;
    set_awe_map((awe_table_t*) awe_map);
}



/*
 * Uninitilaizes awe mapper.
 */
void 
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
    int id = __builtin_ffsll(awe_map->awe_bitmap);
    awe_map->awe_bitmap &= ~(1 << (id - 1));
    return id; 
}  
EXPORT_SYMBOL(awe_mapper_create_id);


/*
 * Returns new available id.
 */
int inline 
awe_mapper_create_id()
{
    awe_table_t *awe_map =  get_awe_map();
    return _awe_mapper_create_id(awe_map); 
}  
EXPORT_SYMBOL(awe_mapper_create_id);

