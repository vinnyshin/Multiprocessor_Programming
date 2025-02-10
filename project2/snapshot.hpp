#pragma once

#include <iostream>
#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <climits>
#include <thread>
#include <unordered_map>
#include <random>
#include <atomic>
#include "snapvalue.hpp"

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
