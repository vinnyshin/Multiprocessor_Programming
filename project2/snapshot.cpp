#include "snapshot.hpp"

Snapshot::Snapshot(size_t num_threads) : num_threads_(num_threads)
{
  gen_ = std::mt19937(rd_());  // Using 'random_device', initialize a random number generator gen_.
  dis_ = std::uniform_int_distribution<int>(0, 99);  // Initialize dis_ uniform distribution with the range of from 0 to 99.

  threads_.reserve(num_threads_);
  registers_.reserve(num_threads_);
  update_cnt_.reserve(num_threads_);

  for (size_t i = 0; i < num_threads_; ++i)
  {
    threads_.emplace_back([this, i]() { this->update(i); }); // Create threads with thread function update.
    /*
      Create shared pointer registers.
      Later, it will automatically free itself if there're no references to itself anymore.
    */
    registers_.emplace_back(std::make_shared<SnapValue>());
    update_cnt_.emplace_back(0);
  }
}

size_t Snapshot::run()
{
  size_t cnt = 0;

  std::unique_lock<std::mutex> lock(m_threads_);
  is_started_ = true;
  /*
    Threads will sleep right after entering update function.
    With is_started_ and cv_threads_, we can guarantee a fair start.
  */
  cv_threads_.notify_all();
  lock.unlock();

  // After waking up all other threads, the main thread falls asleep for 60 seconds.
  std::this_thread::sleep_for(std::chrono::seconds(60));

  // After 60 seconds, the main thread will terminate all threads by setting is_done_ flag to true.
  is_done_ = true;

  for (auto &t : threads_)
  {
    t.join();
  }

  for (size_t value : update_cnt_)
  {
    cnt += value;
  }
  
  // Return with gathered update cnt.
  return cnt;
}

void Snapshot::update(int tid)
{
  std::unique_lock<std::mutex> lock(m_threads_);
  cv_threads_.wait(lock, [this]() { return is_started_; });  // Threads will sleep right after entering update function.
  lock.unlock();

  if (is_done_)
  {
    return;
  }

  while (!is_done_)
  {
    int *snap = scan();  // Get a clean collect with scan function.
    int value = dis_(gen_);  // Get a random number with dis_(gen_).

    snap_shared oldValue = registers_[tid];  // Read Snapvalue from regiters_ using own tid.
    int new_label = (oldValue->label + 1) % UINT_MAX;

    // Make a new shared ptr Snapvalue with the updated label, random number, and clean collect.
    snap_shared newValue = std::make_shared<SnapValue>(new_label, value, snap);

    /* 
      Write the new Snapvalue to the registers_ at tid index.
      Used atomic_store to mimic *AtomicMRSWRegister* behavior.
    */ 
    std::atomic_store(&registers_[tid], newValue);
    ++update_cnt_[tid];
  }

  return;
}

int *Snapshot::scan() {
  snap_shared *oldCopy, *newCopy;
  std::vector<bool> moved(num_threads_, false);
  /*
    Allocate snap with the size of num_threads_.
    Later, it will be automatically freed by SnapValue destructor invoked by shared ptr semantic.
  */
  int *snap = new int[num_threads_];

  oldCopy = collect(); // Get oldCopy, and newCopy respectively.

col:
  while (true) {
    newCopy = collect();

    for (size_t i = 0; i < num_threads_; i++) {
      if (oldCopy[i]->label != newCopy[i]->label) {
        if (moved[i]) {
          // If another update is detected, steal (copy) newCopy->snap values to the snap and return it.
          for (size_t j = 0; j < num_threads_; j++) {
            snap[i] = newCopy[i]->snap[j];
          }

          delete[] oldCopy;
          delete[] newCopy;

          return snap;
        } else {
          // If any update is detected between gathering oldCopy and newCopy, mark moved[i] to true and recollect newCopy.
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

snap_shared *Snapshot::collect() {
  // Make snap_shared temp array.
  snap_shared *copy = new snap_shared[num_threads_];

  /*
    Copy registers_ elements to the temp array.
    Used atomic_load to mimic *AtomicMRSWRegister* behavior.
  */
  for (size_t i = 0; i < num_threads_; i++) {
    copy[i] = std::atomic_load(&registers_[i]);
  }

  return copy;
}