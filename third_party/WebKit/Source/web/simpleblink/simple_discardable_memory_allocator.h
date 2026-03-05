// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_WEB_HTML_VIEWER_SIMPLE_DISCARDABLE_MEMORY_ALLOCATOR_H_
#define THIRD_PARTY_WEBKIT_SOURCE_WEB_HTML_VIEWER_SIMPLE_DISCARDABLE_MEMORY_ALLOCATOR_H_

#include <stddef.h>

#include "base/macros.h"
#include "base/memory/discardable_memory_allocator.h"

namespace html_viewer {

// SimpleDiscardableMemoryAllocator is a simple DiscardableMemoryAllocator
// implementation that can be used for testing. It allocates one-shot
// DiscardableMemory instances backed by heap memory.
class SimpleDiscardableMemoryAllocator : public base::DiscardableMemoryAllocator {
 public:
  SimpleDiscardableMemoryAllocator();
  ~SimpleDiscardableMemoryAllocator() override;

  // Overridden from DiscardableMemoryAllocator:
  scoped_ptr<base::DiscardableMemory> AllocateLockedDiscardableMemory(
      size_t size) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SimpleDiscardableMemoryAllocator);
};

}  // namespace html_viewer

#endif  // THIRD_PARTY_WEBKIT_SOURCE_WEB_HTML_VIEWER_SIMPLE_DISCARDABLE_MEMORY_ALLOCATOR_H_