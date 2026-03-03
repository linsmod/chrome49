// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// HTMLViewer 主程序入口
// 这是一个简单的演示程序，用于测试 Blink Web 封装

#include <stdio.h>
#include <stdlib.h>

#include "web/html_viewer/blink_web_wrapper.h"

using namespace html_viewer;

int main(int argc, char** argv) {
    printf("HTMLViewer - Blink + GLAPP\n");
    printf("Initializing...\n");

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
