// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// HTMLViewer 主程序入口
// 这是一个简单的演示程序，用于测试 Blink Web 封装

#include <stdio.h>
#include <stdlib.h>

#include "web/html_viewer/blink_web_wrapper.h"

#include "base/command_line.h"
#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "base/message_loop/message_loop.h"
#include "base/files/file.h"
#include "wtf/WTF.h"
#include "wtf/MainThread.h"
#include "wtf/Partitions.h"
#include "bindings/core/v8/V8Initializer.h"
#include "gin/public/isolate_holder.h"
#include "gin/array_buffer.h"
#include "gin/v8_initializer.h"
#include "libplatform/libplatform.h"
#include "v8.h"

using namespace v8;

class ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
 public:
  virtual void* Allocate(size_t length) {
    void* data = AllocateUninitialized(length);
    return data == NULL ? data : memset(data, 0, length);
  }
  virtual void* AllocateUninitialized(size_t length) { return malloc(length); }
  virtual void Free(void* data, size_t) { free(data); }
};

using namespace html_viewer;

static double CurrentTime()
{
    return 0.0;
}

int main(int argc, char** argv) {
    printf("HTMLViewer - Blink + GLAPP\n");
    printf("Initializing...\n");

    // 初始化 base 库 (必须在任何其他初始化之前)
    base::CommandLine::Init(argc, argv);
    
    // AtExitManager 管理单例对象的析构
    base::AtExitManager exit_manager;
    
    // 初始化 ICU (国际化支持)
    base::i18n::InitializeICU();
    
    // 初始化 WTF Partitions (内存分区)
    WTF::Partitions::initialize(nullptr);

    // 初始化 WTF (必须在任何 Blink 操作之前)
    WTF::initialize(CurrentTime, nullptr, nullptr, nullptr);
    WTF::initializeMainThread(0);

    // Initialize V8.
    V8::InitializeICU();
    V8::InitializeExternalStartupData(argv[0]);
    v8::Platform* platform = platform::CreateDefaultPlatform();
    V8::InitializePlatform(platform);
    V8::Initialize();

    // Create a new Isolate and make it the current one.
    ArrayBufferAllocator allocator;
    Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = &allocator;
    Isolate* isolate = Isolate::New(create_params);
    
    // 进入 isolate 作用域
    Isolate::Scope isolate_scope(isolate);
    
    // 创建配置
    BlinkWebConfig config = {0};
    config.width = 1024;
    config.height = 768;
    config.enable_js = true;
    config.enable_gpu = false;

    // 初始化 Blink Web
    InitializeBlinkWeb(&config);

    // 创建渲染器
    BlinkWebRenderer* renderer = new BlinkWebRenderer(config.width, config.height);
    
    if (!renderer->Initialize()) {
        printf("Failed to initialize renderer\n");
        return 1;
    }

    // 加载测试 HTML
    const char* test_html = "<html><body><h1>Hello World!</h1></body></html>";
    renderer->LoadHTML(test_html);

    // 渲染一帧
    renderer->Render();

    // 获取像素数据
    uint8_t* pixels = renderer->GetPixels();
    if (pixels) {
        printf("Rendered %dx%d pixels\n", renderer->GetWidth(), renderer->GetHeight());
    }

    // 清理
    renderer->Close();
    delete renderer;
    ShutdownBlinkWeb();

    printf("Done.\n");
    return 0;
}
