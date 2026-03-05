// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "simple_discardable_memory_allocator.h"

#include <stdint.h>

#include "base/memory/discardable_memory.h"

namespace html_viewer {
namespace {

class DiscardableMemoryImpl : public base::DiscardableMemory {
 public:
  explicit DiscardableMemoryImpl(size_t size) : data_(new uint8_t[size]) {}

  // Overridden from DiscardableMemory:
  bool Lock() override { return false; }
  void Unlock() override {}
  void* data() const override { return data_.get(); }

  base::trace_event::MemoryAllocatorDump* CreateMemoryAllocatorDump(
      const char* name,
      base::trace_event::ProcessMemoryDump* pmd) const override {
    return nullptr;
  }

 private:
  scoped_ptr<uint8_t[]> data_;
};

}  // namespace

SimpleDiscardableMemoryAllocator::SimpleDiscardableMemoryAllocator() {
}

SimpleDiscardableMemoryAllocator::~SimpleDiscardableMemoryAllocator() {
}

scoped_ptr<base::DiscardableMemory>
SimpleDiscardableMemoryAllocator::AllocateLockedDiscardableMemory(size_t size) {
  return make_scoped_ptr(new DiscardableMemoryImpl(size));
}

}  // namespace html_viewer