/*
 * awe-mapper.h
 *
 * This file is part of the asynchronous ipc for xcap.
 * This file is responsible for providing mappings
 * between an integer identifier and a pointer to
 * an awe struct.
 *
 * Author: Michael Quigley
 * Date: January 2016 
 */
//#include <stddef.h>
//#include <stdint.h>
//#include <stdbool.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <thc.h>
#include <thcinternal.h>

#ifndef AWE_MAPPER_H
#define AWE_MAPPER_H

/*
 * Initilaizes awe mapper.
 */
void awe_mapper_init(void);

/*
 * Uninitilaizes awe mapper.
 */
void awe_mapper_uninit(void);

/*
 * Returns new available id.
 */
uint32_t awe_mapper_create_id(void);

/*
 * Marks provided id as available
 */
void awe_mapper_remove_id(uint32_t id);

/*
 * Links awe_ptr with id.
 */
void awe_mapper_set_id(uint32_t id, void* awe_ptr);

/*
 * Returns awe_ptr that corresponds to id.
 */
void* awe_mapper_get_awe_ptr(uint32_t id);

static inline awe_table_t* get_awe_map(void)
{
    return current->ptstate->awe_map;
}

static inline void set_awe_map(awe_table_t * map_ptr)
{
    current->ptstate->awe_map = map_ptr;
}



void awe_mapper_print_list(void);

#endif
