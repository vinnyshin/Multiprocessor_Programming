#include "bwtree.h"
#include "bwtree_test_util.h"
#include "multithread_test_util.h"
#include "worker_pool.h"
#include "zipf.h"
#include "timer.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <random>
#include <vector>
#include <string>
#include <mutex>

/*******************************************************************************
 * The test structures stated here were written to give you and idea of what a
 * test should contain and look like. Feel free to change the code and add new
 * tests of your own. The more concrete your tests are, the easier it'd be to
 * detect bugs in the future projects.
 ******************************************************************************/

/*
 * Tests bwtree init/destroy APIs.
 */
TEST(BwtreeInitTest, HandlesInitialization) {
  auto *const tree = test::BwTreeTestUtil::GetEmptyTree();

  ASSERT_TRUE(tree != nullptr);

  delete tree;
}

/*
 * Tests LRU cache init/destroy APIs.
 */
TEST(LRUcacheInitTest, HandlesInitialization) {
  auto *const lru_cache1 = test::BwTreeTestUtil::GetLRUcache(256);
  auto *const lru_cache2 = test::BwTreeTestUtil::GetLRUcache(512);
  auto *const lru_cache3 = test::BwTreeTestUtil::GetLRUcache(1024);
  
  ASSERT_TRUE(lru_cache1 != nullptr);
  ASSERT_TRUE(lru_cache2 != nullptr);
  ASSERT_TRUE(lru_cache3 != nullptr);

  delete lru_cache1;
  delete lru_cache2;
  delete lru_cache3;
}

TEST(LRUcacheHitTest, HitTest) {
  using TreeType =
    bwtree::BwTree<int64_t, int64_t, test::BwTreeTestUtil::KeyComparator, test::BwTreeTestUtil::KeyEqualityChecker>;

  auto *const lru_cache = test::BwTreeTestUtil::GetLRUcache(256);
  
  lru_cache->refer(1, 2);
  ASSERT_TRUE(lru_cache->get(1) != -1); 
  ASSERT_TRUE(lru_cache != nullptr);
  delete lru_cache;
}


/*
 * TestFixture for bwtree tests
 */
class BwtreeTest : public ::testing::Test {
  protected:
    /*
     * NOTE: You can also use constructor/destructor instead of SetUp() and
     * TearDown(). The official document says that the former is actually
     * perferred due to some reasons. Checkout the document for the difference.
     */
    BwtreeTest() {
    }

    ~BwtreeTest() {
    }

    const uint32_t num_threads_ =
      test::MultiThreadTestUtil::HardwareConcurrency() + (test::MultiThreadTestUtil::HardwareConcurrency() % 2);
};

/*
 * Basic functionality test of 1M concurrent random inserts
 */

const uint32_t key_num = 512 * 512;
const uint32_t value_range = 1024 * 1024 * 10;
double alpha = 1.5;
uint32_t cache_size = 2048;
size_t test_cnt = 10;

using TreeType =
    bwtree::BwTree<int64_t, int64_t, test::BwTreeTestUtil::KeyComparator, test::BwTreeTestUtil::KeyEqualityChecker>;

TEST_F(BwtreeTest, ConcurrentRandomInsert) {
  std::vector<double> times;

  for (size_t i = 0; i < test_cnt; i++) {
    // This defines the key space (0 ~ (1M - 1))
    
    std::atomic<size_t> insert_success_counter = 0;
    std::atomic<size_t> update_success_counter = 0;
    std::atomic<size_t> cache_success_counter = 0;
    
    common::WorkerPool thread_pool(num_threads_, {});
    auto *const tree = test::BwTreeTestUtil::GetEmptyTree();
    thread_pool.Startup();
    
    std::vector<std::vector<std::pair<int, int>>> key_val_list(num_threads_, std::vector<std::pair<int, int>>());

    for (size_t i = 0; i < num_threads_; i++) {
      key_val_list[i].reserve(key_num);
    }
    
    // Inserts in a 1M key space randomly until all keys has been inserted
    auto workload_insert = [&](uint32_t id) {
      const uint32_t gcid = id + 1;
      tree->AssignGCID(gcid);
      
      std::default_random_engine thread_generator(id);
      std::uniform_int_distribution<int> uniform_dist(0,  value_range - 1);
      util::Zipf zipf(alpha, key_num);

      uint32_t op_cnt = 0;
      
      while (insert_success_counter.load() < key_num) {
        int value = uniform_dist(thread_generator);
        int key = zipf.Generate();
        // int key = uniform_dist(thread_generator);
        
        if (tree->Insert(key, value)) {
          key_val_list[id].emplace_back(key, value);
          insert_success_counter.fetch_add(1);
        }
        op_cnt++;
      }
    };

    auto workload_update = [&](uint32_t id) {
      std::default_random_engine thread_generator(id);
      std::uniform_int_distribution<int> uniform_dist(0, value_range - 1);
      
      while (update_success_counter.load() < key_num) {
        auto begin = key_val_list[id].begin();
        auto end = key_val_list[id].end();
        
        // random element from the vector
        std::uniform_int_distribution<> dis(0, std::distance(begin, end) - 1);
        std::advance(begin, dis(thread_generator));

        int key = (*begin).first;
        int value = uniform_dist(thread_generator);
      
        if (tree->Insert(key, value)) {
          update_success_counter.fetch_add(1);
        }
      }
      
      delete tree->lru_cache;
    };

    util::Timer<std::milli> timer;
    timer.Start();
    tree->UpdateThreadLocal(num_threads_ + 1);
    test::MultiThreadTestUtil::RunThreadsUntilFinish(&thread_pool, num_threads_, workload_insert);
    tree->UpdateThreadLocal(1);
    timer.Stop();

    std::cout << tree->max_level << std::endl;
    std::cout << i << " insert : "<< timer.GetElapsed() << " (ms)," << " cache hit: " << cache_success_counter << std::endl;

    util::Timer<std::milli> timer2;
    timer2.Start();
    tree->UpdateThreadLocal(num_threads_ + 1);
    test::MultiThreadTestUtil::RunThreadsUntilFinish(&thread_pool, num_threads_, workload_update);
    tree->UpdateThreadLocal(1);
    timer2.Stop();

    std::cout << i << " update : "<< timer2.GetElapsed() << " (ms)," << std::endl;
    times.push_back(timer2.GetElapsed());

    delete tree;
  }

  double temp = 0;
  for (size_t i = 0; i < test_cnt; i++) {
    temp += times[i];
  }
  temp /= test_cnt;

  
  std::cout << "mean time elapsed: "<< temp << std::endl;
}




TEST_F(BwtreeTest, ConcurrentLRURandomInsert) {
  std::vector<double> times;
  
  for (size_t i = 0; i < test_cnt; i++) {
    // This defines the key space (0 ~ (1M - 1))
    
    std::atomic<size_t> insert_success_counter = 0;
    std::atomic<size_t> update_success_counter = 0;
    std::atomic<size_t> cache_success_counter = 0;

    common::WorkerPool thread_pool(num_threads_, {});
    auto *const tree = test::BwTreeTestUtil::GetEmptyTree();
    thread_pool.Startup();
    
    std::vector<std::vector<std::pair<int, int>>> key_val_list(num_threads_, std::vector<std::pair<int, int>>());

    for (size_t i = 0; i < num_threads_; i++) {
      key_val_list[i].reserve(key_num);
    }
    
    // Inserts in a 1M key space randomly until all keys has been inserted
    auto workload_insert = [&](uint32_t id) {
      const uint32_t gcid = id + 1;
      tree->AssignGCID(gcid);
      
      std::default_random_engine thread_generator(id);
      std::uniform_int_distribution<int> uniform_dist(0, key_num - 1);
      util::Zipf zipf(alpha, key_num);

      uint32_t op_cnt = 0;
      
      while (insert_success_counter.load() < key_num) {
        int value = uniform_dist(thread_generator);
        int key = zipf.Generate();
        // int key = uniform_dist(thread_generator);
        
        if (tree->Insert(key, value)) {
          key_val_list[id].emplace_back(key, value);
          insert_success_counter.fetch_add(1);
        }
        op_cnt++;
      }
    };

    auto workload_update = [&](uint32_t id) {
      std::default_random_engine thread_generator(id);
      std::uniform_int_distribution<int> uniform_dist(0, value_range - 1);
      
      tree->lru_cache = test::BwTreeTestUtil::GetLRUcache(cache_size);

      while (update_success_counter.load() < key_num) {
        auto begin = key_val_list[id].begin();
        auto end = key_val_list[id].end();
        
        // random element from the vector
        std::uniform_int_distribution<> dis(0, std::distance(begin, end) - 1);
        std::advance(begin, dis(thread_generator));

        int key = (*begin).first;
        int value = uniform_dist(thread_generator);
      
        if (tree->Insert_LRU(key, value)) {
          key_val_list[id].emplace_back(key, value); // for checking the data consistency 
          update_success_counter.fetch_add(1);
        }
      }
      
      cache_success_counter.fetch_add(tree->lru_cache->cache_hit_success.load());
      delete tree->lru_cache;
    };

    util::Timer<std::milli> timer;
    timer.Start();
    tree->UpdateThreadLocal(num_threads_ + 1);
    test::MultiThreadTestUtil::RunThreadsUntilFinish(&thread_pool, num_threads_, workload_insert);
    tree->UpdateThreadLocal(1);
    timer.Stop();

    std::cout << tree->max_level << std::endl;
    std::cout << i << " insert : "<< timer.GetElapsed() << " (ms)," << std::endl;

    util::Timer<std::milli> timer2;
    timer2.Start();
    tree->UpdateThreadLocal(num_threads_ + 1);
    test::MultiThreadTestUtil::RunThreadsUntilFinish(&thread_pool, num_threads_, workload_update);
    tree->UpdateThreadLocal(1);
    timer2.Stop();

    std::cout << i << " update : "<< timer2.GetElapsed() << " (ms)," << " cache hit: " << cache_success_counter  << std::endl;
    times.push_back(timer2.GetElapsed());

    // To check whether data is consistent uncomment below
    // auto workload_consistency = [&](uint32_t id) {
    //   std::cout << key_val_list[id].size() << std::endl;
    //   for (size_t j = 0; j < key_val_list[id].size(); j++) {
    //     auto kv_pair = key_val_list[id][j];
    //     auto s = tree->GetValue(kv_pair.first);
    //     assert(s.find(kv_pair.second) != s.end());
    //   }
    // };
    
    // tree->UpdateThreadLocal(num_threads_ + 1);
    // test::MultiThreadTestUtil::RunThreadsUntilFinish(&thread_pool, num_threads_, workload_consistency);
    // tree->UpdateThreadLocal(1);
    
    // std::cout << "data consistent!" << std::endl;

    delete tree;
  }

  double temp = 0;
  for (size_t i = 0; i < test_cnt; i++) {
    temp += times[i];
  }
  temp /= test_cnt;

  
  std::cout << "mean time elapsed: "<< temp << std::endl;
}