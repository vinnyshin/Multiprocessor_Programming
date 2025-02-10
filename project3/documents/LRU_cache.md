# How to run

In the terminal

> cd {GIT_DIR}/project3/sourcefile
>
> ./compile.sh
>
> ./run_release.sh



You can manipulate the parameters by changing variables values in ***/test/bwtree_test.cc***.

```c++
// line no 88
const uint32_t key_num = 512 * 512;
const uint32_t value_range = 1024 * 1024 * 10;
double alpha = 1.5;
uint32_t cache_size = 2048;
size_t test_cnt = 10;
```



You can test uniform distribution by uncommenting the ***int key*** in workload_insert and workload_update.

```c++
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
    // uncomment this
    // int key = uniform_dist(thread_generator); 

    if (tree->Insert(key, value)) {
      key_val_list[id].emplace_back(key, value);
      insert_success_counter.fetch_add(1);
    }
    op_cnt++;
  }
};
```



You can change tree height by uncommenting the macros in ***/bwtree/include/bwtree.h***.

```c++
// line no 80
// If node size goes above this then we split it
#define INNER_NODE_SIZE_UPPER_THRESHOLD ((int)128)
#define INNER_NODE_SIZE_LOWER_THRESHOLD ((int)32)

#define LEAF_NODE_SIZE_UPPER_THRESHOLD ((int)128)
#define LEAF_NODE_SIZE_LOWER_THRESHOLD ((int)32)

// uncomment these

//#define INNER_NODE_SIZE_UPPER_THRESHOLD ((int)4)
//#define INNER_NODE_SIZE_LOWER_THRESHOLD ((int)1)

//#define LEAF_NODE_SIZE_UPPER_THRESHOLD ((int)4)
//#define LEAF_NODE_SIZE_LOWER_THRESHOLD ((int)1)
```



# Implementation

## Class Definition

```c++
/* Inside of class BwTree */
// ...
  class LRUcache {
    private:    
      std::list<std::pair<int, NodeID>> dq;
      std::unordered_map<int, typename std::list<std::pair<int, NodeID>>::iterator > ma;
      int cache_size;

    public:
      std::atomic<uint64_t> cache_hit_trial;
      std::atomic<uint64_t> cache_hit_fail;
      std::atomic<uint64_t> cache_hit_success;

      LRUcache(int size): cache_size(size), cache_hit_success(0) {}
      ~LRUcache() {}

      NO_ASAN bool refer(int key, NodeID parent_node_id) {
        if(ma.find(key) == ma.end()) { // not present in cache
          if(dq.size() == cache_size) {
              int last_key = dq.back().first;
              dq.pop_back();
              ma.erase(last_key);
          }
        } else { // present in cache
          dq.erase(ma[key]);
        }
        
        dq.push_front(std::pair(key, parent_node_id));
        ma[key] = dq.begin();
        
        return true;
      }

      NO_ASAN NodeID get(int key) {
        if(ma.find(key) == ma.end()) { // not present in cache
          return -1;
        } else { // present in cache
          return (*ma[key]).second;
        }
      }
  };

  inline static thread_local LRUcache* lru_cache;

// ...
```

- LRU cache stores the recent access info in the list
- By saving the list iterator in the unordered map, we can get the node_id with time complexity O(1).
- Each thread has its own LRU cache so there's no need to consider any contention between the caches.



## Traverse_LRU

```c++
NO_ASAN const KeyValuePair *Traverse_LRU(Context *context_p, const ValueType *value_p, std::pair<int, bool> *index_pair_p, bool unique_key = false) {
    // For value collection it always returns nullptr
    
    const KeyValuePair *found_pair_p = nullptr;
    NodeID child_node_id;
    NodeID start_node_id;
  
    NodeID parent_node_id = lru_cache->get(context_p->search_key);

    if (parent_node_id != -1) {
      lru_cache->cache_hit_success++;
      context_p->current_snapshot.node_id = parent_node_id;
      context_p->current_snapshot.node_p = GetNode(parent_node_id);
      goto cache_hit;
    }
		
	  // ...
  
    while (1) {
cache_hit:
      child_node_id = NavigateInnerNode(context_p);
      
      // ...
    }
```

- If we can find the corresponding target parent node id, set the current snapshot with it.
- Find the target child node id with NavigateInnerNode.



## Insert_LRU

```c++
NO_ASAN bool Insert_LRU(const KeyType &key, const ValueType &value, bool unique_key = false) {
	// ...
      bool ret = InstallNodeToReplace(node_id, insert_node_p, node_p);
      if (ret) {
        INDEX_LOG_TRACE("Leaf Insert delta CAS succeed");
        INDEX_LOG_TRACE("[node_id: %d]size of node: %d", node_id, node_p->GetItemCount());
        
        lru_cache->refer(key, GetLatestParentNodeSnapshot(&context)->node_id);
        
        // If install is a success then just break from the loop
        // and return
        break;
      }
      INDEX_LOG_TRACE("Leaf insert delta CAS failed");
  
	// ...
  }
```

- If CAS succeeds, add it to the lru_cache.



# Experiments

I conducted a performance evaluation with the following configurations.

Even for the vanilla Bw-tree implementation, in uniform distribution settings, if we set the split threshold small sometimes it gives us a segmentation fault error.

I couldn't find what was the problem.

Because it was a vanilla Bw-tree implementation error and an occasional error, I ignored it and proceeded to the experiments.



## Datasets

- Skewed(alpha: 1.5) 0.25M, split threshold 128, no LRU_cache
- Skewed(alpha: 1.5) 0.25M, split threshold 128, yes LRU_cache
- Skewed(alpha: 1.5) 0.25M, split threshold 4, no LRU_cache
- Skewed(alpha: 1.5) 0.25M, split threshold 4, yes LRU_cache
- Skewed(alpha: 2.0) 0.25M, split threshold 128, no LRU_cache
- Skewed(alpha: 2.0) 0.25M, split threshold 128, yes LRU_cache
- Skewed(alpha: 2.0) 0.25M, split threshold 4, no LRU_cache
- Skewed(alpha: 2.0) 0.25M, split threshold 4, yes LRU_cache
- Uniform 1M, split threshold 128, no LRU_cache
- Uniform 1M, split threshold 128, yes LRU_cache
- Uniform 1M, split threshold 4, no LRU_cache
- Uniform 1M, split threshold 4, yes LRU_cache



Because of the nature of the B-like tree, the height won't that high.

I manipulated tree height by forcibly setting the upper split threshold large and small to see the performance difference.



For the skewed distribution, if we insert 1M keys into the tree, the time for each iteration becomes too long.

Therefore I set the total insert count at 0.25M.



On the other hand, the one with uniform distribution wasn't that long, I set the total insert count at 1M.



I set the cache size to 2048.

From the small experiments, I found that if the cache size is above 2048 it won't affect that much to the performance.



## low-height settings

| Skewed(alpha: 1.5) 0.25M, split threshold 128, no LRU_cache | Skewed(alpha: 1.5) 0.25M, split threshold 128, yes LRU_cache |
| :---------------------------------------------------------: | :----------------------------------------------------------: |
|                                                             |                                                              |
|             Mean update time (outlier excluded)             |             Mean update time (outlier excluded)              |
|                           30869.2                           |                           31381.2                            |

- height = 2
- The performance difference was 1.7%



| Skewed(alpha: 2.0) 0.25M, split threshold 128, no LRU_cache | Skewed(alpha: 2.0) 0.25M, split threshold 128, yes LRU_cache |
| :---------------------------------------------------------: | :----------------------------------------------------------: |
|                                                             |                                                              |
|             Mean update time (outlier excluded)             |             Mean update time (outlier excluded)              |
|                           57101.2                           |                           57811.3                            |

- height = 2
- The performance difference was 1%



| Uniform 1M, split threshold 128, no LRU_cache | Uniform 1M, split threshold 128, yes LRU_cache |
| :-------------------------------------------: | :--------------------------------------------: |
|                                               |                                                |
|      Mean update time (outlier excluded)      |      Mean update time (outlier excluded)       |
|                     282.6                     |                     278.1                      |

- height = 2
- The difference was 4ms (which can be interpreted as noise)



If the height was low, there was no benefit to using an LRU cache.

The LRU cache is devised to reduce a single traverse function call time (from invocation to return).

However, if the height is low, there are few inner nodes to traverse and the single function call time will be small.



## high-height settings

| Skewed(alpha: 1.5) 0.25M, split threshold 4, no LRU_cache | Skewed(alpha: 1.5) 0.25M, split threshold 4, yes LRU_cache |
| :-------------------------------------------------------: | :--------------------------------------------------------: |
|                                                           |                                                            |
|            Mean update time (outlier excluded)            |            Mean update time (outlier excluded)             |
|                          32111.5                          |                          30149.8                           |

- height = 5
- Performance was 6.5% increased with LRU_cache



| Skewed(alpha: 2.0) 0.25M, split threshold 4, no LRU_cache | Skewed(alpha: 2.0) 0.25M, split threshold 4, yes LRU_cache |
| :-------------------------------------------------------: | :--------------------------------------------------------: |
|                                                           |                                                            |
|            Mean update time (outlier excluded)            |            Mean update time (outlier excluded)             |
|                          59053.8                          |                          57462.5                           |

- height = 4
- The performance was 2.7% increased with LRU_cache



| Uniform 1M, split threshold 4, no LRU_cache | Uniform 1M, split threshold 4, yes LRU_cache |
| :-----------------------------------------: | :------------------------------------------: |
|                                             |                                              |
|     Mean update time (outlier excluded)     |     Mean update time (outlier excluded)      |
|                    423.1                    |                   404.355                    |

- height = 8
- The performance was 4.7% increased with LRU_cache



We can see a small performance increase in ***Skewed(alpha: 1.5) split threshold 4, Skewed(alpha: 2.0) split threshold 4, and Uniform split threshold 4***.

Because they have a relatively high height compared to ***low-height settings*** cases.



***Skewed(alpha: 2.0) split threshold 4*** has a low height compared to ***Skewed(alpha: 1.5) split threshold 4***.

it's because The more the distribution is skewed, the fewer split nodes with a new key.



Although uniform distribution cannot utilize temporal locality well, we get the performance increase.

It's because, still, we can skip a few traverses.

The uniform distribution's height was high(= 8).

So even skipping a few traverses reduced total traverse time.



The more height increases, the more benefit we get from the LRU cache increases along.



As I said the LRU cache is devised to reduce a single traverse function call time (from invocation to return), and we can conclude that it successfully reduces it.



# Conclusion

We can get a performance increase with the LRU cache, but the effect is not that dramatic.

It's because of the nature of the B-like tree. the height won't that high.



It could be more effective to reduce the number of total traverses(i.e. design #3) than reduce a single traverse call time.
