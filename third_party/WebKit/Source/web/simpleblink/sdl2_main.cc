// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// HTMLViewer SDL2 主程序入口
// 使用 SDL2 窗口显示 Blink 软件渲染结果

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "web/simpleblink/blink_web_wrapper.h"

#include "base/command_line.h"
#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "base/message_loop/message_loop.h"
#include "base/files/file.h"
#include "base/thread_task_runner_handle.h"
#include "base/path_service.h"
#include "base/memory/discardable_memory_allocator.h"
#include "base/test/test_discardable_memory_allocator.h"

// Blink headers
#include "public/web/WebKit.h"
#include "public/platform/Platform.h"
#include "wtf/Partitions.h"

// Gin headers for V8 initialization
#include "gin/v8_initializer.h"

// V8 headers
#include "libplatform/libplatform.h"
#include "v8.h"

// SDL2 headers
#include <SDL.h>  // BUILD.gn 中已添加 /usr/include/SDL2 到 include_dirs

#include "simple_discardable_memory_allocator.h"

using namespace v8;
using namespace html_viewer;

// V8 snapshot blob 文件路径
#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
static const char kNativesBlobPath[] = "/home/wulin/chrome49/src/out/Release/natives_blob.bin";
static const char kSnapshotBlobPath[] = "/home/wulin/chrome49/src/out/Release/snapshot_blob.bin";
#endif

int main(int argc, char** argv) {
    printf("HTMLViewer SDL2 - Blink Software Rendering\n");
    printf("Initializing...\n");

    // 1. 初始化 base 库 (必须在任何其他初始化之前)
    base::CommandLine::Init(argc, argv);
    
    // AtExitManager 管理单例对象的析构
    base::AtExitManager exit_manager;
    
    // 1.5 初始化 DiscardableMemoryAllocator (Skia 需要)
    SimpleDiscardableMemoryAllocator discardable_memory_allocator;
    base::DiscardableMemoryAllocator::SetInstance(&discardable_memory_allocator);
    
    // 2. 创建 MessageLoop (Blink 需要)
    base::MessageLoop message_loop;
    
    // 3. 初始化 WTF Partitions (内存分区) - 必须在 ICU 之前
    WTF::Partitions::initialize(nullptr);
    
    // 4. 初始化 ICU (国际化支持)
    base::i18n::InitializeICU();

#if defined(V8_USE_EXTERNAL_STARTUP_DATA)
    // 4.5 使用 gin 加载 V8 snapshot blob 文件
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

    // 5. 初始化 SDL2
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    
    // 6. 创建窗口
    int windowWidth = 1024;
    int windowHeight = 768;
    
    SDL_Window* window = SDL_CreateWindow(
        "HTMLViewer - Blink Software Rendering",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        windowWidth, windowHeight,
        SDL_WINDOW_SHOWN
    );
    
    if (!window) {
        printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    
    // 7. 创建渲染器
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) {
        printf("SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    // 8. 创建纹理 (用于显示 Blink 渲染结果)
    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA8888,  // Blink 输出 RGBA
        SDL_TEXTUREACCESS_STREAMING,
        windowWidth, windowHeight
    );
    
    if (!texture) {
        printf("SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // 11. 创建渲染器 (会调用 blink::initialize)
    BlinkWebRenderer* blinkRenderer = new BlinkWebRenderer(800, 600);
    
    if (!blinkRenderer->Initialize()) {
        printf("Failed to initialize renderer\n");
        delete blinkRenderer;
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // 12. 加载测试 HTML
    const char* test_html = R"HTML(
<!DOCTYPE html>
<html>
<head>
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 20px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
        }
        h1 {
            color: white;
            text-shadow: 2px 2px 4px rgba(0,0,0,0.5);
            font-size: 48px;
        }
        .card {
            background: white;
            border-radius: 10px;
            padding: 20px;
            margin: 20px 0;
            box-shadow: 0 4px 6px rgba(0,0,0,0.1);
        }
        .card h2 {
            color: #333;
            margin-top: 0;
        }
        .card p {
            color: #666;
            line-height: 1.6;
        }
        button {
            background: #667eea;
            color: white;
            border: none;
            padding: 10px 20px;
            border-radius: 5px;
            cursor: pointer;
            font-size: 16px;
        }
        button:hover {
            background: #764ba2;
        }
    </style>
</head>
<body>
    <h1>🎉 Blink Software Rendering</h1>
    <div class="card">
        <h2>Hello from HTMLViewer!</h2>
        <p>This is a test page rendered using Blink's software rendering mode.</p>
        <p>No GPU or compositor required - pure CPU rendering via Skia.</p>
        <button onclick="alert('Button clicked!')">Click Me</button>
    </div>
    <div class="card">
        <h2>Features</h2>
        <p>✅ HTML5 parsing</p>
        <p>✅ CSS styling</p>
        <p>✅ JavaScript (V8)</p>
        <p>✅ Software rendering</p>
    </div>
</body>
</html>
)HTML";
    
    blinkRenderer->LoadHTML(test_html);
    printf("HTML loaded successfully!\n");

    // 13. 渲染一帧
    blinkRenderer->Render();

    // 14. 获取像素数据并显示
    uint8_t* pixels = blinkRenderer->GetPixels();
    int pitch = windowWidth * 4;  // 移到外面，供后续使用
    if (pixels) {
        printf("Rendered %dx%d pixels\n", blinkRenderer->GetWidth(), blinkRenderer->GetHeight());
        
        // 更新 SDL 纹理
        SDL_UpdateTexture(texture, NULL, pixels, pitch);
        
        // 渲染到窗口
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

    // 15. 主循环 - 等待用户关闭窗口
    printf("Window displayed. Press ESC or close window to exit.\n");
    
    bool running = true;
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        running = false;
                    }
                    // 传递键盘事件到 Blink
                    blinkRenderer->HandleKeyDown(event.key.keysym.sym);
                    break;
                case SDL_KEYUP:
                    blinkRenderer->HandleKeyUp(event.key.keysym.sym);
                    break;
                case SDL_MOUSEMOTION:
                    blinkRenderer->HandleMouseMove(event.motion.x, event.motion.y);
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    blinkRenderer->HandleMouseDown(event.button.x, event.button.y, event.button.button);
                    // 重新渲染
                    blinkRenderer->Render();
                    SDL_UpdateTexture(texture, NULL, blinkRenderer->GetPixels(), pitch);
                    SDL_RenderClear(renderer);
                    SDL_RenderCopy(renderer, texture, NULL, NULL);
                    SDL_RenderPresent(renderer);
                    break;
                case SDL_MOUSEBUTTONUP:
                    blinkRenderer->HandleMouseUp(event.button.x, event.button.y, event.button.button);
                    break;
                case SDL_MOUSEWHEEL:
                    blinkRenderer->HandleMouseWheel(event.wheel.x, event.wheel.y, event.wheel.y);
                    break;
            }
        }
        
        // 小延迟避免 CPU 占用过高
        SDL_Delay(16);  // ~60 FPS
    }

    // 16. 清理
    blinkRenderer->Close();
    delete blinkRenderer;

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    printf("Done.\n");
    return 0;
}