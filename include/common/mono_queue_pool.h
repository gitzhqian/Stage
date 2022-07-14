//===----------------------------------------------------------------------===//
//
//                         Peloton
//
// mono_queue_pool.h
//
// Identification: src/include/threadpool/mono_queue_pool.h
//
// Copyright (c) 2015-2018, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include "constants.h"
#include "worker_pool.h"

namespace mvstore {
/**
 * @brief Wrapper class for single queue and single pool.
 */
class MonoQueuePool {
 public:
  MonoQueuePool(const std::string &name, uint32_t task_queue_size,
                uint32_t worker_pool_size);

  ~MonoQueuePool();

  void Startup();

  void Shutdown();

  template <typename F>
  void SubmitTask(const F &func);

  uint32_t NumWorkers() const { return worker_pool_.NumWorkers(); }

  /// Instances for various components
  static MonoQueuePool &GetInstance();

 private:
  TaskQueue task_queue_;
  WorkerPool worker_pool_;
  bool is_running_;
};

////////////////////////////////////////////////////////////////////////////////
///
/// Implementation
///
////////////////////////////////////////////////////////////////////////////////

inline MonoQueuePool::MonoQueuePool(const std::string &name,
                                    uint32_t task_queue_size,
                                    uint32_t worker_pool_size)
    : task_queue_(task_queue_size),
      worker_pool_(name, worker_pool_size, task_queue_),
      is_running_(false) {}

inline MonoQueuePool::~MonoQueuePool() {
  if (is_running_) {
    Shutdown();
  }
}

inline void MonoQueuePool::Startup() {
  worker_pool_.Startup();
  is_running_ = true;
}

inline void MonoQueuePool::Shutdown() {
  worker_pool_.Shutdown();
  is_running_ = false;
}

template <typename F>
inline void MonoQueuePool::SubmitTask(const F &func) {
  if (!is_running_) {
    Startup();
  }
  task_queue_.Enqueue(std::move(func));
}

inline MonoQueuePool &MonoQueuePool::GetInstance() {
  int32_t task_queue_size =  monoqueue_task_queue_size;
  int32_t worker_pool_size = monoqueue_worker_pool_size;

  PELOTON_ASSERT(task_queue_size > 0,"task_queue_size > 0");
  PELOTON_ASSERT(worker_pool_size > 0,"worker_pool_size > 0");

  std::string name = "main-pool";

  static MonoQueuePool mono_queue_pool(name,
                                       static_cast<uint32_t>(task_queue_size),
                                       static_cast<uint32_t>(worker_pool_size));
  return mono_queue_pool;
}

}
