#pragma once

#include "cachelib/allocator/CacheAllocator.h"


using namespace facebook::cachelib;
// using Cache = facebook::cachelib::LruAllocator;
using Cache = Lru2QAllocator;
// using Cache = TinyLFUAllocator;


void mycache_init(int64_t cache_size_in_mb, unsigned int hashpower,
                       Cache **cache_p, PoolId *pool_p);

int cache_get(Cache *cache, PoolId pool, struct request *req);

int cache_set(Cache *cache, PoolId pool, struct request *req);

int cache_del(Cache *cache, PoolId pool, struct request *req);

double cache_utilization(Cache *cache);
