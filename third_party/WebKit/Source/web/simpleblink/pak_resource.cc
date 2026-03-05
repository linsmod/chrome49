// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// 简单的 PAK 资源加载器，不依赖 ui 库
// 参考 ui/base/resource/data_pack.cc

#include "web/simpleblink/pak_resource.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace html_viewer {

// PAK 文件格式版本
static const uint32_t kFileFormatVersion = 4;

// 条目比较函数 (用于 bsearch)
static int CompareEntryById(const void* void_key, const void* void_entry) {
    uint16_t key = *reinterpret_cast<const uint16_t*>(void_key);
    const PakResource::Entry* entry =
        reinterpret_cast<const PakResource::Entry*>(void_entry);
    if (key < entry->resource_id) {
        return -1;
    } else if (key > entry->resource_id) {
        return 1;
    } else {
        return 0;
    }
}

PakResource::PakResource()
    : mmap_data_(nullptr)
    , mmap_size_(0)
    , resource_count_(0)
    , entries_(nullptr)
    , fd_(-1) {
}

PakResource::~PakResource() {
    if (mmap_data_ && mmap_data_ != MAP_FAILED) {
        munmap(mmap_data_, mmap_size_);
    }
    if (fd_ >= 0) {
        close(fd_);
    }
}

bool PakResource::LoadFromFile(const std::string& path) {
    // 打开文件
    fd_ = open(path.c_str(), O_RDONLY);
    if (fd_ < 0) {
        fprintf(stderr, "PakResource: Failed to open %s\n", path.c_str());
        return false;
    }

    // 获取文件大小
    struct stat st;
    if (fstat(fd_, &st) < 0) {
        fprintf(stderr, "PakResource: Failed to stat %s\n", path.c_str());
        close(fd_);
        fd_ = -1;
        return false;
    }
    mmap_size_ = st.st_size;

    // 内存映射
    mmap_data_ = mmap(nullptr, mmap_size_, PROT_READ, MAP_PRIVATE, fd_, 0);
    if (mmap_data_ == MAP_FAILED) {
        fprintf(stderr, "PakResource: Failed to mmap %s\n", path.c_str());
        close(fd_);
        fd_ = -1;
        mmap_data_ = nullptr;
        return false;
    }

    // 解析文件头
    if (kHeaderLength > mmap_size_) {
        fprintf(stderr, "PakResource: File too small for header\n");
        return false;
    }

    const uint8_t* data = reinterpret_cast<const uint8_t*>(mmap_data_);

    // 读取版本号
    uint32_t version;
    memcpy(&version, data, sizeof(version));
    if (version != kFileFormatVersion) {
        fprintf(stderr, "PakResource: Bad version %u, expected %u\n",
                version, kFileFormatVersion);
        return false;
    }

    // 读取资源数量
    memcpy(&resource_count_, data + 4, sizeof(resource_count_));

    // 跳过编码类型 (1 字节)

    // 设置条目表指针
    entries_ = reinterpret_cast<const Entry*>(data + kHeaderLength);

    // 验证条目表
    // Entry 大小是 6 字节 (uint16_t + uint32_t)
    uint32_t index_length = (resource_count_ + 1) * 6;
    if (kHeaderLength + index_length > mmap_size_) {
        fprintf(stderr, "PakResource: Index truncated (header=%zu, index=%u, total=%zu, file=%u)\n",
                kHeaderLength, index_length, (size_t)(kHeaderLength + index_length), mmap_size_);
        return false;
    }

    return true;
}

bool PakResource::HasResource(uint16_t resource_id) const {
    if (!entries_ || resource_count_ == 0) {
        return false;
    }
    return bsearch(&resource_id, entries_, resource_count_,
                   6, CompareEntryById) != nullptr;
}

bool PakResource::GetResource(uint16_t resource_id, const char** data, size_t* size) const {
    if (!entries_ || resource_count_ == 0) {
        return false;
    }

    const Entry* target = reinterpret_cast<const Entry*>(
        bsearch(&resource_id, entries_, resource_count_,
                6, CompareEntryById));
    if (!target) {
        return false;
    }

    // 下一个条目用于计算大小 (每个条目 6 字节)
    const Entry* next_entry = reinterpret_cast<const Entry*>(
        reinterpret_cast<const uint8_t*>(target) + 6);
    if (next_entry->file_offset > mmap_size_) {
        fprintf(stderr, "PakResource: Entry points off end of file\n");
        return false;
    }

    const uint8_t* base = reinterpret_cast<const uint8_t*>(mmap_data_);
    *data = reinterpret_cast<const char*>(base + target->file_offset);
    *size = next_entry->file_offset - target->file_offset;
    return true;
}

}  // namespace html_viewer