// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// HTMLViewer 主程序入口
// 这是一个简单的演示程序，用于测试 Blink Web 封装

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "web/html_viewer/blink_web_wrapper.h"

#include "base/command_line.h"
#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "base/message_loop/message_loop.h"
#include "base/files/file.h"
#include "base/thread_task_runner_handle.h"
#include "base/path_service.h"

// Blink headers
#include "public/web/WebKit.h"
#include "public/platform/Platform.h"
#include "wtf/Partitions.h"

// Gin headers for V8 initialization
#include "gin/v8_initializer.h"

// V8 headers
#include "libplatform/libplatform.h"
#include "v8.h"

using namespace v8;
using namespace html_viewer;

// V8 snapshot blob 文件路径
#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
static const char kNativesBlobPath[] = "/home/wulin/chrome49/src/out/Release/natives_blob.bin";
static const char kSnapshotBlobPath[] = "/home/wulin/chrome49/src/out/Release/snapshot_blob.bin";
#endif

int main(int argc, char** argv) {
    printf("HTMLViewer - Blink + GLAPP\n");
    printf("Initializing...\n");

    // 1. 初始化 base 库 (必须在任何其他初始化之前)
    base::CommandLine::Init(argc, argv);
    
    // AtExitManager 管理单例对象的析构
    base::AtExitManager exit_manager;
    
    // 2. 创建 MessageLoop (Blink 需要)
    base::MessageLoop message_loop;
    
    // 3. 初始化 WTF Partitions (内存分区) - 必须在 ICU 之前
    WTF::Partitions::initialize(nullptr);
    
    // 4. 初始化 ICU (国际化支持)
    base::i18n::InitializeICU();

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
    // 4.5 使用 gin 加载 V8 snapshot blob 文件
    // 参考 components/html_viewer/global_state.cc
    base::File natives_file(base::FilePath::FromUTF8Unsafe(kNativesBlobPath),
                            base::File::FLAG_OPEN | base::File::FLAG_READ);
    base::File snapshot_file(base::FilePath::FromUTF8Unsafe(kSnapshotBlobPath),
                             base::File::FLAG_OPEN | base::File::FLAG_READ);
    
    if (!natives_file.IsValid()) {
        printf("Failed to open natives_blob.bin: %s\n", kNativesBlobPath);
        return 1;
    }
    if (!snapshot_file.IsValid()) {
        printf("Failed to open snapshot_blob.bin: %s\n", kSnapshotBlobPath);
        return 1;
    }
    
    gin::V8Initializer::LoadV8NativesFromFD(natives_file.TakePlatformFile(), 0u, 0u);
    gin::V8Initializer::LoadV8SnapshotFromFD(snapshot_file.TakePlatformFile(), 0u, 0u);
    printf("V8 blobs loaded successfully\n");
#endif

    // 5. 创建配置
    BlinkWebConfig config = {0};
    config.width = 1024;
    config.height = 768;
    config.enable_js = true;
    config.enable_gpu = false;

    // 6. 初始化 Blink Web (创建 Platform)
    InitializeBlinkWeb(&config);

    // 7. 创建渲染器 (会调用 blink::initialize)
    BlinkWebRenderer* renderer = new BlinkWebRenderer(config.width, config.height);
    
    if (!renderer->Initialize()) {
        printf("Failed to initialize renderer\n");
        return 1;
    }

    // 8. 加载测试 HTML
    const char* test_html = "<html><body><h1>Hello World!</h1></body></html>";
    renderer->LoadHTML(test_html);

    // 9. 渲染一帧
    renderer->Render();

    // 10. 获取像素数据
    uint8_t* pixels = renderer->GetPixels();
    if (pixels) {
        printf("Rendered %dx%d pixels\n", renderer->GetWidth(), renderer->GetHeight());
    }

    // 11. 清理
    renderer->Close();
    delete renderer;
    ShutdownBlinkWeb();

    printf("Done.\n");
    return 0;
}