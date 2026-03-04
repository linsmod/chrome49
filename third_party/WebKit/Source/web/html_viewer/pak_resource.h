// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// 简单的 PAK 资源加载器，不依赖 ui 库
// 参考 ui/base/resource/data_pack.cc

#ifndef WEB_HTML_VIEWER_PAK_RESOURCE_H_
#define WEB_HTML_VIEWER_PAK_RESOURCE_H_

#include <stddef.h>
#include <stdint.h>
#include <string>

namespace html_viewer {

// 简单的 PAK 文件资源加载器
class PakResource {
public:
    PakResource();
    ~PakResource();

    // 从文件路径加载 PAK 文件
    bool LoadFromFile(const std::string& path);

    // 检查是否有指定资源
    bool HasResource(uint16_t resource_id) const;

    // 获取资源数据，返回数据指针和大小
    // 注意：返回的指针在 PakResource 对象生命周期内有效
    bool GetResource(uint16_t resource_id, const char** data, size_t* size) const;

    // PAK 文件条目结构 (6 字节)
    #pragma pack(push, 2)
    struct Entry {
        uint16_t resource_id;
        uint32_t file_offset;
    };
    #pragma pack(pop)

private:

    // 文件头长度: version(4) + count(4) + encoding(1) = 9
    static const size_t kHeaderLength = 9;

    // 内存映射数据
    void* mmap_data_;
    uint32_t mmap_size_;

    // 资源数量
    uint32_t resource_count_;

    // 条目表指针
    const Entry* entries_;

    // 文件描述符 (用于 munmap)
    int fd_;
};

}  // namespace html_viewer

#endif  // WEB_HTML_VIEWER_PAK_RESOURCE_H_