/*
 * awe_mapper.h
 *
 * This file is part of the asynchronous ipc for xcap.
 * This file is responsible for providing mappings
 * between an integer identifier and a pointer to
 * an awe struct.
 *
 * Author: Michael Quigley
 * Date: January 2016 
 */
#ifndef AWE_MAPPER_H
#define AWE_MAPPER_H

#include <thc.h>

#define AWE_TABLE_ORDER 10
#define AWE_TABLE_COUNT (1 << AWE_TABLE_ORDER)

struct awe_table
{
    void* awe_list[AWE_TABLE_COUNT];
    uint32_t used_slots;
    uint32_t next_id;
};

typedef struct awe_table awe_table_t;

/*
 * Initilaizes awe mapper.
 */
void awe_mapper_init(void);

/*
 * Uninitilaizes awe mapper.
 */
void awe_mapper_uninit(void);

/*
 * Returns new available id as out param.
 *
 * Returns non-zero on failure.
 */
int awe_mapper_create_id(uint32_t *new_id);


static inline awe_table_t* get_awe_map(void)
{
    return PTS()->awe_map;
}

/*
 * Called in awe_mapper_init.
 * Sets awe_map struct of the current PTS to a specific awe_table.
 */
static inline void set_awe_map(awe_table_t * map_ptr)
{
    PTS()->awe_map = map_ptr;
}

/*
 * Marks provided id as available
 */
static inline void
LIBASYNC_FUNC_ATTR 
awe_mapper_remove_id(uint32_t id)
{
    awe_table_t *awe_map =  get_awe_map();
    
    assert(id < AWE_TABLE_COUNT);
    
    if( awe_map->used_slots > 0 )
    {
        awe_map->used_slots--;
    }
    
    (awe_map->awe_list)[id] = NULL;
}

/*
 * Links awe_ptr with id.
 */
static inline void
awe_mapper_set_id(uint32_t id, void* awe_ptr)
{
    awe_table_t *awe_map =  get_awe_map();
    assert(id < AWE_TABLE_COUNT);
    (awe_map->awe_list)[id] = awe_ptr;
}



/*
 * Returns awe_ptr that corresponds to id.
 */
static inline void*
LIBASYNC_FUNC_ATTR 
awe_mapper_get_awe_ptr(uint32_t id)
{
    awe_table_t *awe_map = get_awe_map();
    if (id >= AWE_TABLE_COUNT)
        return NULL;
    return awe_map->awe_list[id];
}

static inline void*
LIBASYNC_FUNC_ATTR 
awe_mapper_get_awe_ptr_trusted(uint32_t id)
{
    awe_table_t *awe_map = get_awe_map();
    return awe_map->awe_list[id];
}

#endif
