#pragma once

#include "cachelib/allocator/CacheAllocator.h"

using namespace facebook::cachelib;
#if defined(USE_LRU) || defined(USE_STRICTLRU)
using Cache = facebook::cachelib::LruAllocator;
#elif defined(USE_CLOCK)
using Cache = facebook::cachelib::ClockAllocator;
#elif defined(USE_ATOMICCLOCK)
using Cache = facebook::cachelib::AtomicClockAllocator;
#elif defined(USE_ATOMICCLOCKBUFFERED)
using Cache = facebook::cachelib::AtomicClockBufferedAllocator;
#elif defined(USE_S3FIFO)
using Cache = S3FIFOAllocator;
#elif defined(USE_TWOQ)
using Cache = Lru2QAllocator;
#elif defined(USE_TINYLFU)
using Cache = TinyLFUAllocator;
#endif

void mycache_init(int64_t cache_size_in_mb, unsigned int hashpower,
                  Cache **cache_p, PoolId *pool_p);

int cache_get(Cache *cache, PoolId pool, struct request *req);

int cache_set(Cache *cache, PoolId pool, struct request *req);

int cache_del(Cache *cache, PoolId pool, struct request *req);

double cache_utilization(Cache *cache);
