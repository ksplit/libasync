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

#include <linux/bug.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <awe_mapper.h>

#ifdef LCD_DOMAINS
#include <lcd_config/post_hook.h>
#endif

#ifndef LINUX_KERNEL_MODULE
#undef EXPORT_SYMBOL
#define EXPORT_SYMBOL(x)
#endif

/*
 * NOTE: This implementation right now is just a ring buffer.
 * In the future, we probably want to change this to something
 * like a red black tree or a B-tree to account for differing
 * storage size requirements.
 */

/*
 * This value is used to determine if a slot is allocated but not yet set in the awe table.
 */
static unsigned long initialized_marker = 0xDeadBeef;

awe_table_t* global_awe_map;

/*
 * Initilaizes awe mapper.
 */
void 
LIBASYNC_FUNC_ATTR 
awe_mapper_init(void)
{
    void* awe_map_ptr = kzalloc(sizeof(awe_table_t), GFP_KERNEL);
    if( !awe_map_ptr )
    {
        pr_err("No space left for awe_map_ptr\n");
        return;
    }

    set_awe_map((awe_table_t*) awe_map_ptr);
}



/*
 * Uninitilaizes awe mapper.
 */
void 
LIBASYNC_FUNC_ATTR 
awe_mapper_uninit(void)
{
    printk(KERN_ERR "in awe_mapper_uninit\n");
    awe_table_t *awe_map =  get_awe_map();
    kfree(awe_map);
}



static bool is_slot_allocated(uint32_t id)
{
    awe_table_t *awe_map =  get_awe_map();
    return ((awe_map->awe_list)[id] != NULL);
}



/*
 * Returns new available id.
 */
int
LIBASYNC_FUNC_ATTR 
awe_mapper_create_id(uint32_t *new_id)
{
    printk(KERN_ERR "before awe_map\n");
    awe_table_t *awe_map =  get_awe_map();
    printk(KERN_ERR "after awe_map = %lx\n", (unsigned long)awe_map);

    if (unlikely(awe_map->used_slots >= AWE_TABLE_COUNT))
    {
        printk(KERN_ERR "awe_mapper_create_id: too many slots requested\n");
        return -ENOMEM;
    }
    
    do
    {
        awe_map->next_id = (awe_map->next_id + 1) % AWE_TABLE_COUNT;
    } 
    while( is_slot_allocated(awe_map->next_id) );

    awe_map->awe_list[awe_map->next_id] = (void*)initialized_marker;

    awe_map->used_slots++;

    *new_id = awe_map->next_id;

    return 0;
}  
EXPORT_SYMBOL(awe_mapper_create_id);

