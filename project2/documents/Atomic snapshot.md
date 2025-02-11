#	Atomic snapshot

An **atomic snapshot object** acts like a collection of n single-writer multi-reader atomic registers with a special **snapshot** operation that returns (what appears to be) the state of all n registers simultaneously.

This is easy without failures: we simply lock the whole register file, read them all, and unlock them to let all the starving writers in. 

But it gets harder if we want a wait-free protocol, where any process can finish its snapshot or write even if all the others lock up.



# Implementation

## How to run

From the terminal



> cd project2/
>
> make
>
> ./run N



## SnapValue

```c++
class SnapValue {
public:
  SnapValue(size_t label_ = 0, int value_ = 0, int* snap_ = nullptr)
    : label(label_), value(value_), snap(snap_) {}
	
  size_t label;  // time stamp
  int value;     // used for random value update
  int* snap;     // clean collect

  ~SnapValue() {
    if (snap != nullptr) {
      delete[] snap;    
    }
  }
};

```



## Snapshot

```c++
typedef std::shared_ptr<SnapValue> snap_shared;

class Snapshot {
 public:
  Snapshot(size_t num_threads);
  void update(int tid);
  int* scan();
  size_t run();

 private:
  snap_shared* collect();

  std::condition_variable cv_threads_;  // used for fair start
  std::mutex m_threads_;

  bool is_done_ = false;
  bool is_started_ = false;  // used for fair start
  size_t num_threads_;

  std::vector<std::thread> threads_;
  std::vector<snap_shared> registers_;
  std::vector<size_t> update_cnt_;
  std::random_device rd_;
  std::mt19937 gen_;
  std::uniform_int_distribution<int> dis_;  // random number generator
};
```



## Constructor

```c++
Snapshot::Snapshot(size_t num_threads) : num_threads_(num_threads) {
  gen_ = std::mt19937(rd_());
  dis_ = std::uniform_int_distribution<int>(0, 99);

  threads_.reserve(num_threads_);
  registers_.reserve(num_threads_);
  update_cnt_.reserve(num_threads_);

  for (size_t i = 0; i < num_threads_; ++i) {
    threads_.emplace_back([this, i]() { this->update(i); });
    registers_.emplace_back(std::make_shared<SnapValue>());
    update_cnt_.emplace_back(0);
  }
}
```

1. Using *'random_device'*, initialize a random number generator **gen_**.
2. Initialize **dis_** uniform distribution with the range of from 0 to 99.
3. Create threads with thread function **update**.
4. Create shared pointer registers.
   1. Later, it will automatically free itself if there're no references to itself anymore.



## Snapshot::run()

```c++
size_t Snapshot::run() {
  size_t cnt = 0;

  std::unique_lock<std::mutex> lock(m_threads_);
  is_started_ = true;
  cv_threads_.notify_all();
  lock.unlock();
  
  std::this_thread::sleep_for(std::chrono::seconds(60));

  is_done_ = true;
  
  for (auto& t : threads_) {
    t.join();
  }

  for (size_t value : update_cnt_) {
    cnt += value;
  }

  return cnt;
}
```

1. Threads will sleep right after entering **update** function.
   1. With **is_started_** and **cv_threads_**, we can guarantee a fair start.
2. After waking up all other threads, the main thread falls asleep for 60 seconds.
3. After 60 seconds, the main thread will terminate all threads by setting **is_done_** flag to true.
4. Return with gathered update cnt.



## Snapshot::update()

```c++
void Snapshot::update(int tid) {
  std::unique_lock<std::mutex> lock(m_threads_);
  cv_threads_.wait(lock, [this]() { return is_started_; });
  lock.unlock();

  if (is_done_) {
      return;
  }

  while (!is_done_) {
    int* snap = scan();
    int value = dis_(gen_);
    
    snap_shared oldValue = registers_[tid];
    int new_label = (oldValue->label + 1) % UINT_MAX;

    snap_shared newValue = std::make_shared<SnapValue>(new_label, value, snap);
    
    std::atomic_store(&registers_[tid], newValue);
    ++update_cnt_[tid];
  }
  
  return;
}

```

1. Get a clean collect with **scan** function.
2. Get a random number with **dis\_(gen\_**).
3. Read Snapvalue from **regiters_** using own tid.
4. Make a new shared ptr Snapvalue with the updated label, random number, and clean collect.

5. Write the new Snapvalue to the **registers_** at tid index.
   1. Used **atomic_store** to mimic *AtomicMRSWRegister* behavior.



## Snapshot::collect()

```c++
snap_shared *Snapshot::collect() {
  snap_shared *copy = new snap_shared[num_threads_];

  for (size_t i = 0; i < num_threads_; i++) {
    copy[i] = std::atomic_load(&registers_[i]);
  }

  return copy;
}
```

1. Make **snap_shared** temp array.
2. Copy **registers_** elements to the temp array.
   1. Used **atomic_load** to mimic *AtomicMRSWRegister* behavior.



## Snapshot::scan()

```c++
int *Snapshot::scan() {
  snap_shared *oldCopy, *newCopy;
  std::vector<bool> moved(num_threads_, false);
  int *snap = new int[num_threads_];

  oldCopy = collect();

col:
  while (true) {
    newCopy = collect();

    for (size_t i = 0; i < num_threads_; i++) {
      if (oldCopy[i]->label != newCopy[i]->label) {
        if (moved[i]) {
          for (size_t j = 0; j < num_threads_; j++) {
            snap[i] = newCopy[i]->snap[j];
          }

          delete[] oldCopy;
          delete[] newCopy;

          return snap;
        } else {
          moved[i] = true;
          delete[] oldCopy;
          oldCopy = newCopy;
          goto col;
        }
      }
    }

    for (size_t i = 0; i < num_threads_; i++) {
      snap[i] = newCopy[i]->value;
    }

    delete[] oldCopy;
    delete[] newCopy;

    return snap;
  }
}
```

1. Allocate **snap** with the size of **num_threads_**.
   1. Later, it will be automatically freed by **SnapValue destructor** invoked by shared ptr semantic.
2. Get **oldCopy**, and **newCopy** respectively.
3. If any update is detected between gathering **oldCopy** and **newCopy**, mark moved[i] to true and recollect **newCopy**.
   1. If not, copy **newCopy** values to the **snap** and return it.
4. If another update is detected, steal (copy) **newCopy->snap** values to the **snap** and return it.
   1. If not, copy **newCopy** values to the **snap** and return it.



# Discussion

### Test 1
![test1](https://github.com/vinnyshin/Multiprocessor_Programming/blob/main/project2/results/test1.png)

### Test 2
![test2](https://github.com/vinnyshin/Multiprocessor_Programming/blob/main/project2/results/test2.png)

### Test 3
![test3](https://github.com/vinnyshin/Multiprocessor_Programming/blob/main/project2/results/test3.png)


I analyzed the performance relationship between the number of threads and update counts.

Under the number of 2 threads, the performance is slightly increased.

This is because the total throughput increase from parallelization is more significant than the performance decrease from snapshot update while scanning to get an atomic snapshot.

Over the number of 2 threads, the performance is decreased.

This is because the total throughput increase from parallelization is smaller than the performance decrease from snapshot update while scanning to get an atomic snapshot.

If the number of threads gets larger, it gets a higher possibility of update conflict during scanning.



# Reference

yale.edu: AtomicSnapshots (https://www.cs.yale.edu/homes/aspnes/pinewiki/AtomicSnapshots.html)
