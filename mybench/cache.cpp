

// #include <fmt/core.h>
// #include <folly/Conv.h>
// #include <folly/FBString.h>

// using namespace folly;

#include "cachelib/allocator/LruTailAgeStrategy.h"
#include "cachelib/allocator/MarginalHitsStrategy.h"
#include "cachelib/allocator/RebalanceStrategy.h"
#include "cachelib/common/Time.h"

#include <chrono>

#include "request.h"
#include "cache.h"


static void print_config(Cache::Config &config) {
  printf(
      "poolRebalancingEnabled %d, poolOptimizerEnabled %d, "
      "itemsReaperEnabled %d\n",
      config.poolRebalancingEnabled(), config.poolOptimizerEnabled(),
      config.itemsReaperEnabled());

  auto configmap = config.serialize();
  for (auto &kv : configmap) {
    printf("%s %s\n", kv.first.c_str(), kv.second.c_str());
  }
}

void mycache_init(int64_t cache_size_in_mb, unsigned int hashpower,
                       Cache **cache_p, PoolId *pool_p) {
  Cache::Config config;

  auto rebalance_strategy = std::make_shared<LruTailAgeStrategy>();
  auto rebalance_strategy2 = std::make_shared<MarginalHitsStrategy>();

  config.setCacheSize(cache_size_in_mb * 1024 * 1024)
      .setCacheName("My cache")
      // .enableItemReaperInBackground(std::chrono::seconds(1), {})
      // .enablePoolRebalancing(rebalance_strategy, std::chrono::seconds(1))
      .setAccessConfig({hashpower, 20})
      .validate();

  *cache_p = new Cache(config);
  *pool_p = (*cache_p)->addPool("default",
                                (*cache_p)->getCacheMemoryStats().ramCacheSize);

  // Cache::MMConfig mm_config;
  // mm_config.lruInsertionPointSpec = 2;
  // *pool_p = (*cache_p)->addPool("default",
  //                               (*cache_p)->getCacheMemoryStats().ramCacheSize);

  // Cache::MMConfig mm_config;
  // mm_config.lruRefreshTime = 0;
  // *pool_p = (*cache_p)->addPool("default",
  //                               (*cache_p)->getCacheMemoryStats().ramCacheSize,
  //                               {}, mm_config);

  util::setCurrentTimeSec(1);
  assert(util::getCurrentTimeSec() == 1);
}

static inline std::string gen_key(struct request *req) {
  // auto key = std::string(req->key);
  auto key = fmt::format_int(*(uint64_t *)(req->key)).str();
  // auto key = to<fbstring>(*(uint64_t *)(req->key));
  // printf("key %s %s\n", req->key, key.c_str());

  return key;
}

int cache_get(Cache *cache, PoolId pool, struct request *req) {
  // static __thread char buf[1024 * 1024];

  // util::setCurrentTimeSec(req->timestamp);

  auto key = gen_key(req);

  Cache::ReadHandle item_handle = cache->find(key);
  if (item_handle) {
    // printf("%d obj %s expireTiem %d %d %d + %ld\n",
    // util::getCurrentTimeSec(), req->key, item_handle->getExpiryTime(),
    // item_handle->isExpired(), item_handle->getCreationTime(),
    // item_handle->getConfiguredTTL());

    if (item_handle->isExpired()) {
      return 1;
    } else {
      assert(item_handle->getSize() == req->val_len);
      assert(!item_handle->isExpired());
      const char *data =
          reinterpret_cast<const char *>(item_handle->getMemory());
      // memcpy(buf, data, req->val_len);

      return 0;
    }
  } else {
    return 1;
  }
}

int cache_set(Cache *cache, PoolId pool, struct request *req) {
  // util::setCurrentTimeSec(req->timestamp);

  auto key = gen_key(req);

  req->val_len += req->key_len - key.size();
  if (req->val_len < 0) req->val_len = 0;

  Cache::WriteHandle item_handle =
      cache->allocate(pool, key, req->val_len, req->ttl, req->timestamp);
  if (item_handle == nullptr || item_handle->getMemory() == nullptr) {
    // std::cout << "allocate error " << std::string(req->key) << std::endl;
    // usleep(2000);
    return 1;
  }

  std::memcpy(item_handle->getMemory(), req->val, req->val_len);
  cache->insertOrReplace(item_handle);

  return 0;
}

int cache_del(Cache *cache, PoolId pool, struct request *req) {
  // util::setCurrentTimeSec(req->timestamp);

  auto key = gen_key(req);
  auto rm = cache->remove(key);
  if (rm == Cache::RemoveRes::kSuccess)
    return 0;
  else
    return 1;
}

double cache_utilization(Cache *cache) {
  int64_t used_size = 0;
  for (const auto &itr : *cache) {
    auto key = itr.getKey();
    used_size += key.size() + itr.getSize();
  }

  return (double)used_size / (double)cache->getCacheMemoryStats().ramCacheSize;
}
