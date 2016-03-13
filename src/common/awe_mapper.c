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
 * Initilaizes awe mapper.
 */
void awe_mapper_init(void)
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
void awe_mapper_uninit(void)
{
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
uint32_t awe_mapper_create_id(void)
{
    awe_table_t *awe_map =  get_awe_map();
    BUG_ON((awe_map->used_slots >= AWE_TABLE_COUNT) && "Too many slots have been requested.");
    
    do
    {
        awe_map->next_id = (awe_map->next_id + 1) % AWE_TABLE_COUNT;
    } 
    while( is_slot_allocated(awe_map->next_id) );

    (awe_map->awe_list)[awe_map->next_id] = (void*)0xdeadbeef;

    awe_map->used_slots++;

    return awe_map->next_id;
}  
EXPORT_SYMBOL(awe_mapper_create_id);


/*
 * Marks provided id as available
 */
void awe_mapper_remove_id(uint32_t id)
{
    awe_table_t *awe_map =  get_awe_map();
    BUG_ON(id >= AWE_TABLE_COUNT);
    
    if( awe_map->used_slots > 0 )
    {
        awe_map->used_slots--;
    }
    
    (awe_map->awe_list)[id] = NULL;
}
EXPORT_SYMBOL(awe_mapper_remove_id);


/*
 * Links awe_ptr with id.
 */
void awe_mapper_set_id(uint32_t id, void* awe_ptr)
{
    awe_table_t *awe_map =  get_awe_map();
    BUG_ON(id >= AWE_TABLE_COUNT);
    (awe_map->awe_list)[id] = awe_ptr;
}



/*
 * Returns awe_ptr that corresponds to id.
 */
void* awe_mapper_get_awe_ptr(uint32_t id)
{
    awe_table_t *awe_map =  get_awe_map();
    BUG_ON(id >= AWE_TABLE_COUNT);

    return (awe_map->awe_list)[id];
}


void awe_mapper_print_list(void)
{
    int i,j;
    awe_table_t *awe_map =  get_awe_map();
    for(i = 0; i < AWE_TABLE_COUNT; i+= 8)
    {
        printk(KERN_ERR "\n");
        for(j = i; j < i + 8; j++)
        {
            printk(KERN_ERR "0x%p ", (awe_map->awe_list)[j]);
        }
    }
    printk(KERN_ERR "\n");
}
EXPORT_SYMBOL(awe_mapper_print_list);
