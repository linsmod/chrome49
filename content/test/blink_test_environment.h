// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_BLINK_TEST_ENVIRONMENT_H_
#define CONTENT_TEST_BLINK_TEST_ENVIRONMENT_H_
#include "base/message_loop/message_loop.h"
// This package provides functions used by webkit_unit_tests.
namespace content {
#if defined(OS_ANDROID)
  // Android UI message loop goes through Java, so don't use it in tests.
  typedef base::MessageLoop MessageLoopType;
#else
  typedef base::MessageLoopForUI MessageLoopType;
#endif    

// Initializes Blink test environment for unit tests.
void SetUpBlinkTestEnvironment();

MessageLoopType* SetUpBlinkTestEnvironmentAndGetMainMessageLoop();

// Terminates Blink test environment for unit tests.
void TearDownBlinkTestEnvironment();

}  // namespace content

#endif  // CONTENT_TEST_BLINK_TEST_ENVIRONMENT_H_
