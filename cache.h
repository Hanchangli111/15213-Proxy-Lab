/*Aayush Karki
 Andrew : aayushka*/

#ifndef __CACHE_H__
#define __CACHE_H__

#include "csapp.h"

#define MAX_CACHE_SIZE  1049000
#define MAX_OBJECT_SIZE 102400

#define INIT_MAX_ENTRIES 64
#define EXTEND_CHUNK 128
#define  eLISTSIZE (1<<8)

struct file
{
	char *key;
	char *data;
	int fsize;
	unsigned short timestamp;
};



int getCacheInfo(int n);

struct file *retrieve(char *filename);

void extendCache();

int evict_many(int size);

void init_evictedList();

int get_freeSpot();

void add2cache(char *filename, char *objfile, int len);

struct file *init_cache();

void erase_cache();


#endif /* __CACHE_H__ */
