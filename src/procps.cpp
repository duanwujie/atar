#include "libbb.h"


typedef struct id_to_name_map_t {
	uid_t id;
	char name[USERNAME_MAX_SIZE];
} id_to_name_map_t;

typedef struct cache_t {
	id_to_name_map_t *cache;
	int size;
} cache_t;

static cache_t username, groupname;

static void clear_cache(cache_t *cp)
{
	free(cp->cache);
	cp->cache = NULL;
	cp->size = 0;
}
void FAST_FUNC clear_username_cache(void)
{
	clear_cache(&username);
	clear_cache(&groupname);
}

static char* get_cached(cache_t *cp, uid_t id,
			char* FAST_FUNC x2x_utoa(uid_t id))
{
	int i;
	for (i = 0; i < cp->size; i++)
		if (cp->cache[i].id == id)
			return cp->cache[i].name;
	i = cp->size++;
	cp->cache = (id_to_name_map_t *)xrealloc_vector(cp->cache, 2, i);
	cp->cache[i].id = id;
	/* Never fails. Generates numeric string if name isn't found */
	safe_strncpy(cp->cache[i].name, x2x_utoa(id), sizeof(cp->cache[i].name));
	return cp->cache[i].name;
}
const char* FAST_FUNC get_cached_username(uid_t uid)
{
	return get_cached(&username, uid, uid2uname_utoa);
}
const char* FAST_FUNC get_cached_groupname(gid_t gid)
{
	return get_cached(&groupname, gid, gid2group_utoa);
}
