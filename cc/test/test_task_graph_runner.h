// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_TEST_TASK_GRAPH_RUNNER_H_
#define CC_TEST_TEST_TASK_GRAPH_RUNNER_H_

#include "base/macros.h"
#include "base/threading/simple_thread.h"
#include "cc/raster/single_thread_task_graph_runner.h"
#include <mutex> 
namespace cc {

class TestTaskGraphRunner : public SingleThreadTaskGraphRunner {
 public:
  static TestTaskGraphRunner* Instance() {  
    std::lock_guard<std::mutex> lock(mutex_);  
    if (!s_instance) {  
      s_instance = new TestTaskGraphRunner();  
    }  
    return s_instance;  
  }  
  
  ~TestTaskGraphRunner() override;

 private:
  TestTaskGraphRunner();
  static TestTaskGraphRunner* s_instance;  
  static std::mutex mutex_;  
  DISALLOW_COPY_AND_ASSIGN(TestTaskGraphRunner);
};

}  // namespace cc

#endif  // CC_TEST_TEST_TASK_GRAPH_RUNNER_H_
