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
//assert(AWE_TABLE_ORDER == 6); 

struct awe_table
{
    awe_t awe_list[AWE_TABLE_COUNT];
    long long awe_bitmap;

//    uint32_t used_slots;
//    uint32_t next_id;
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
void awe_mapper_uninit_with_pts(void *pts);

/*
 * Returns new available id as out param.
 *
 * Returns non-zero on failure.
 */
int awe_mapper_create_id(int *);


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

static inline void
_awe_mapper_remove_id(awe_table_t *awe_map, uint32_t id)
{
    assert(id <= AWE_TABLE_COUNT);
    assert(!(awe_map->awe_bitmap & (1 << (id - 1))));
    awe_map->awe_bitmap |= (1 << (id - 1)); 
}

/*
 * Marks provided id as available
 */
static inline void
LIBASYNC_FUNC_ATTR 
awe_mapper_remove_id(uint32_t id)
{
    awe_table_t *awe_map =  get_awe_map();
    _awe_mapper_remove_id(awe_map, id); 
}

//static inline void
//_awe_mapper_set_id(awe_table_t *awe_map, uint32_t id, void* awe_ptr)
//{
//    assert(id < AWE_TABLE_COUNT);
//    (awe_map->awe_list)[id] = awe_ptr;
//}

/*
 * Links awe_ptr with id.
 */
//static inline void
//awe_mapper_set_id(uint32_t id, void* awe_ptr)
//{
//    awe_table_t *awe_map =  get_awe_map();
//    _awe_mapper_set_id(awe_map, id, awe_ptr);
//}
static inline int _is_slot_allocated(awe_table_t *awe_map, uint32_t id)
{
    return !(awe_map->awe_bitmap & (1 << (id - 1)));
}

static inline int is_slot_allocated(uint32_t id)
{
    awe_table_t *awe_map =  get_awe_map();
    return _is_slot_allocated(awe_map, id);
}

static inline awe_t *
_awe_mapper_get_awe(awe_table_t *awe_map, uint32_t id)
{

    //assert(id >= AWE_TABLE_COUNT);
    if (id > AWE_TABLE_COUNT) {
	printk("%s, id(%u) > table count(%u)\n", __func__, id, AWE_TABLE_COUNT);
    }
    if (!awe_map) {
	printk("%s, awemap for id %d = %p\n", __func__, id, awe_map);
#ifdef LCD_DOMAINS
	BUG();
#endif
	return NULL;
    }
    if (!_is_slot_allocated(awe_map, id))
	return NULL;   
    assert(id <= AWE_TABLE_COUNT);
    return &(awe_map->awe_list[id]);
}


/*
 * Returns awe_ptr that corresponds to id.
 */
static inline awe_t*
LIBASYNC_FUNC_ATTR 
awe_mapper_get_awe(uint32_t id)
{
    awe_table_t *awe_map = get_awe_map();
    if (!awe_map) {
	printk("%s, awe_map for id %d = %p\n", __func__, id, awe_map);
#ifdef LCD_DOMAINS
	BUG();
#endif
        return NULL;
    }
#ifdef LCD_DOMAINS
//    printk("%s, awe_map %p | pts()->awemap %p, awemap2 %p | id %d\n",
//		__func__, awe_map, PTS()->awe_map, PTS()->awe_map2, id);
#endif
    return _awe_mapper_get_awe(awe_map, id);
}

static inline awe_t *
LIBASYNC_FUNC_ATTR 
_awe_mapper_get_awe_ptr_trusted(awe_table_t *awe_map, uint32_t id)
{
    return &(awe_map->awe_list[id]);
}

static inline awe_t*
LIBASYNC_FUNC_ATTR 
awe_mapper_get_awe_ptr_trusted(uint32_t id)
{
    awe_table_t *awe_map = get_awe_map();
    return _awe_mapper_get_awe_ptr_trusted(awe_map, id);
}

#endif
