// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_RENDER_VIEW_TEST_COPY_H_
#define CONTENT_PUBLIC_TEST_RENDER_VIEW_TEST_COPY_H_
#include "content/public/test/render_view_test.h"
namespace content{
  CONTENT_EXPORT RenderViewTest* createRenderViewTest(base::MessageLoop* message_loop_for_app_ui);
}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_RENDER_VIEW_TEST_H_
