// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SIMPLE_WEB_TASK_RUNNER_H_
#define SIMPLE_WEB_TASK_RUNNER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "third_party/WebKit/public/platform/WebTaskRunner.h"

namespace html_viewer {

// WebTaskRunner 实现，包装 base::SingleThreadTaskRunner
// 移植自 components/scheduler/child/web_task_runner_impl.h
class SimpleWebTaskRunner : public blink::WebTaskRunner {
public:
    explicit SimpleWebTaskRunner(
        scoped_refptr<base::SingleThreadTaskRunner> task_runner);
    ~SimpleWebTaskRunner() override;

    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner() const {
        return task_runner_;
    }

    // blink::WebTaskRunner implementation:
    void postTask(const blink::WebTraceLocation& web_location,
                  blink::WebTaskRunner::Task* task) override;
    void postDelayedTask(const blink::WebTraceLocation& web_location,
                         blink::WebTaskRunner::Task* task,
                         double delayMs) override;
    blink::WebTaskRunner* clone() override;

    // Helper to run blink::WebTaskRunner::Task from scoped_ptr
    static void runTask(scoped_ptr<blink::WebTaskRunner::Task> task);

private:
    scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

    DISALLOW_COPY_AND_ASSIGN(SimpleWebTaskRunner);
};

}  // namespace html_viewer

#endif  // SIMPLE_WEB_TASK_RUNNER_H_
