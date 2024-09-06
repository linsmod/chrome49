// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_task_graph_runner.h"

namespace cc {
TestTaskGraphRunner* TestTaskGraphRunner::s_instance = nullptr;  
std::mutex TestTaskGraphRunner::mutex_; 
TestTaskGraphRunner::TestTaskGraphRunner() {
  Start("TestTaskGraphRunner", base::SimpleThread::Options());
}

TestTaskGraphRunner::~TestTaskGraphRunner() {
  Shutdown();
}

}  // namespace cc
