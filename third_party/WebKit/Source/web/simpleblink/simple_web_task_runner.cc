// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/simpleblink/simple_web_task_runner.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "third_party/WebKit/public/platform/WebTraceLocation.h"

namespace html_viewer {

SimpleWebTaskRunner::SimpleWebTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(task_runner) {}

SimpleWebTaskRunner::~SimpleWebTaskRunner() {}

void SimpleWebTaskRunner::postTask(const blink::WebTraceLocation& web_location,
                                   blink::WebTaskRunner::Task* task) {
    tracked_objects::Location location(web_location.functionName(),
                                       web_location.fileName(), -1, nullptr);
    task_runner_->PostTask(
        location,
        base::Bind(&SimpleWebTaskRunner::runTask,
                   base::Passed(scoped_ptr<blink::WebTaskRunner::Task>(task))));
}

void SimpleWebTaskRunner::postDelayedTask(
    const blink::WebTraceLocation& web_location,
    blink::WebTaskRunner::Task* task,
    double delayMs) {
    tracked_objects::Location location(web_location.functionName(),
                                       web_location.fileName(), -1, nullptr);
    task_runner_->PostDelayedTask(
        location,
        base::Bind(&SimpleWebTaskRunner::runTask,
                   base::Passed(scoped_ptr<blink::WebTaskRunner::Task>(task))),
        base::TimeDelta::FromMillisecondsD(delayMs));
}

blink::WebTaskRunner* SimpleWebTaskRunner::clone() {
    return new SimpleWebTaskRunner(task_runner_);
}

void SimpleWebTaskRunner::runTask(scoped_ptr<blink::WebTaskRunner::Task> task) {
    task->run();
}

}  // namespace html_viewer
