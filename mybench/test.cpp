#include <fstream>
#include <iostream>

#include "cachelib/allocator/CacheAllocator.h"

using Cache = facebook::cachelib::LruAllocator;

// std::shared_ptr<RebalanceStrategy> CacheConfig::getRebalanceStrategy() {
//   if (poolRebalanceIntervalSec == 0) {
//     return nullptr;
//   }

//   if (rebalanceStrategy == "tail-age") {
//     auto config = LruTailAgeStrategy::Config{
//         rebalanceDiffRatio, static_cast<unsigned int>(rebalanceMinSlabs)};
//     return std::make_shared<LruTailAgeStrategy>(config);
//   } else if (rebalanceStrategy == "hits") {
//     auto config = HitsPerSlabStrategy::Config{
//         rebalanceDiffRatio, static_cast<unsigned int>(rebalanceMinSlabs)};
//     return std::make_shared<HitsPerSlabStrategy>(config);
//   } else {
//     // use random strategy to just trigger some slab release.
//     return std::make_shared<RandomStrategy>(
//         RandomStrategy::Config{static_cast<unsigned
//         int>(rebalanceMinSlabs)});
//   }
// }

using namespace std;
void initializeCache() {
  // cachelib::LruTailAgeStrategy::Config cfg(ratio,
  // kLruTailAgeStrategyMinSlab); cfg.slabProjectionLength = 0; // dont project
  // or estimate tail age cfg.numSlabsFreeMem = 10;

  // auto rebalanceStrategy =
  //     std::make_shared<cachelib::LruTailAgeStrategy>(rebalanceConfig);

  Cache::Config config;
  config.setCacheSize(200 * 1024 * 1024)
      .setCacheName("My cache")
      .setAccessConfig({25, 10})
      .validate();

  // config.enablePoolRebalancing(rebalanceStrategy,
  //                              std::chrono::seconds(kRebalanceIntervalSecs));

  auto cache = std::make_unique<Cache>(config);
  facebook::cachelib::PoolId pool;
  pool = cache->addPool("default", cache->getCacheMemoryStats().cacheSize);

  string data("new data");
  Cache::ItemHandle item_handle = cache->allocate(pool, "key1", 102400);
  std::memcpy(item_handle->getWritableMemory(), data.data(), data.size());
  cache->insert(item_handle);
  // cache->insertOrReplace(item_handle);

  data = "Repalce the data associated with key key1";
  item_handle = cache->allocate(pool, "key1", data.size());
  std::memcpy(item_handle->getWritableMemory(), data.data(), data.size());
  cache->insertOrReplace(item_handle);

  item_handle = cache->find("key1");
  if (item_handle) {
    auto data = reinterpret_cast<const char *>(item_handle->getMemory());
    std::cout << data << '\n';
  }

  // cache.reset();
}

int main(int argc, char *argv[]) {
  initializeCache();

  printf("Hello World\n");
}
