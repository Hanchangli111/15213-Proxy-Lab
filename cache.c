/*Aayush Karki
 Andrew : aayushka*/

#include "csapp.h"
#include "cache.h"
#include <assert.h>



int maxcache_entries=0;
int cache_size = 0;
int cache_entries =0;

struct file *Cache=NULL;
int Evicted_list[eLISTSIZE];
int e_idx =0;

/*****************************************************************************
				Cache Design description
******************************************************************************
1. Maintain a list Evicted_list[LISTSIZES] that holds indices to objects
evicted from the cache. e_idx is always the index to the immediate next spot
in Evicted_list that is -1 (which it is not holding any index of an evicted
spot in the list). This means the array starting at lower indices.
	Eg: Evicted_list[7] = [4,5,6, -1, 8, -1, -1]
									   ^i
				 e_idx =i+1 where i is the index of the rightmost non -1. 
				 Hence here e_idx = 5

    LRU policy is used for eviction.
    This list helps to find which indices in the cache are available.
    After looking for an available spot, the arra

2. The cache initially can hold up to INIT_MAX_ENTRIES number of cached 
objects. Once this limit is reached, the cache size extends to be able to
hold EXTEND_CHUNK more entries.

3. At all times, the size of all cached objects is kept under MAX_CACHE_SIZE, 
and no object is larger than MAX_OBJECT_SIZE.



****************************************************************************/


/*
* Searches the cache and returns the pointer to the object referenced by key.
* Return null if key not found in cache.
*/
struct file *retrieve(char *key){
	int i;
	struct file *unit = NULL;
	for (i = 0; i < cache_entries; ++i)
	{
		if (Cache[i].key != NULL){
			if (!strcmp(key, Cache[i].key)){
				unit = &Cache[i];
				Cache[i].timestamp = 0;
			}
			else{
				//incr timestamp for LRU policy
				Cache[i].timestamp += 1; 
			}

		}//outer if
	}//for
	return unit;
}


/*
* Increase the #entries (objects) that can be held by cache.
*/
void extendCache(){
	struct file *tmp = Cache;

	maxcache_entries += EXTEND_CHUNK;
	Cache =realloc(Cache, maxcache_entries );
	assert(Cache == tmp);
	return ;
}

/*
*Keep evicting LRU objects unless the cache has enough capacity to store
* 'size' object/data
*/
int evict_many(int size){
	int i;
	int max = 0;
	int fsize = Cache[max].fsize;	

	//find the LRU index
	for ( i=1 ; i < cache_entries; ++i)
	{
		if ((fsize<=Cache[i].fsize)
			&&(Cache[i].key !=NULL)){
				max = i;
				fsize = Cache[i].fsize;
			}
	}

	cache_entries -= 1;
	cache_size -= Cache[max].fsize;

	free(Cache[max].data);
	free(Cache[max].key);
	Cache[max].data = NULL;
	Cache[max].key = NULL;

	//recurse if not enough space cleared for size
	if (size > MAX_CACHE_SIZE - cache_size ){
		Evicted_list[e_idx] = max;
		e_idx++;
		if (e_idx == eLISTSIZE) 
			e_idx = 0;
		return evict_many(size);
	}
	else{
		return max;
	}

}

/*
Initializes Evicted_list.
*/
void init_evictedList(){
	int i;
	
	for (i = 0; i < eLISTSIZE; ++i)
	{
		Evicted_list[i] = -1;
	}
	return;
}


/*
* Return the first index at which there is -1 in the Evicted_list
*/
int get_freeSpot(){
	int i;

	for ( i = 0; i < eLISTSIZE; ++i)
	{
		if (Evicted_list[i] != -1){
			Evicted_list[i]= -1;
			return i;
		}
	}
	printf("%s\n", "get_freeSpot Error: sudnt be here" );
	return -1;
}


/*
* Add key:data to Cache
*/

void add2cache(char *key, char *data, int objlen){
	int i;
	/* If Evicted_list if not full, then e_idx points to an index that
	doesnt hold -1. If full, get such index via get_freeSpot()
	*/

	if( (e_idx >0) || (Evicted_list[eLISTSIZE-1] != -1) ) {
		i= get_freeSpot();
	}
	else{
		i = cache_entries;
	}


	int fsize = objlen;
	if (fsize > MAX_OBJECT_SIZE) return;

	//if cache entry limit is reached, increase the cache size and entry limit.
	if (maxcache_entries == cache_entries) {
		extendCache();
	}

	//get free index via eviction if new object size exceeds the net capacity
	//of the cache.
	if (cache_size + fsize> MAX_CACHE_SIZE) {		
		i = evict_many(fsize);
	}


	cache_entries +=1;
	
	cache_size += fsize;
	
	/*Copy info to Cache block*/
	Cache[i].fsize = fsize;
	int key_len = strlen(key);
	Cache[i].key = (char*)malloc(key_len);
	strncpy(Cache[i].key, key, key_len);
	Cache[i].timestamp = 0;

	Cache[i].data = (char*)malloc(fsize);
	memcpy(Cache[i].data,  data, objlen);
	
	retrieve(key); //call retrieve just to update timestamp
	return;	
}

/*
*Initializes cache
*/
struct file *init_cache(){
	maxcache_entries = INIT_MAX_ENTRIES;
	Cache = (struct file *) malloc(sizeof(struct file)*maxcache_entries);
	init_evictedList();

	return Cache;
}

/*
*Frees every pointer used inside Cache
*/
void erase_cache(){
	
	if (Cache==NULL) return;

	int i;
	for ( i = 0; i < cache_entries; ++i)
	{
		if(Cache[i].data != NULL){
				free(Cache[i].data);
			}

		if(Cache[i].key != NULL)
				free(Cache[i].key);
	}

	free(Cache);
	Cache = NULL;
	maxcache_entries=0;
	cache_entries=0;
	cache_size=0;
	exit(0);
}
