// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLINK_WEB_WRAPPER_H_
#define BLINK_WEB_WRAPPER_H_

#include <stdint.h>
#include <stdbool.h>
#include <string>
#include <memory>

// 前向声明
namespace blink {
class WebView;
class WebViewImpl;
class WebFrame;
class WebInputEvent;
class WebLayerTreeView;
class TestingPlatformSupportWithMockScheduler;
}

namespace html_viewer {

// 配置结构体
struct BlinkWebConfig {
    int width;
    int height;
    const char* user_agent;
    bool enable_js;
    bool enable_gpu;
    
    // 渲染完成回调
    void (*on_frame_ready)(uint8_t* pixels, int width, int height, void* user_data);
    void* user_data;
};

// 主渲染器类
class BlinkWebRenderer {
public:
    explicit BlinkWebRenderer(int width, int height);
    ~BlinkWebRenderer();

    // 初始化 Blink
    bool Initialize();

    // 加载内容
    void LoadHTML(const std::string& html);
    void LoadURL(const std::string& url);

    // 渲染控制
    void Resize(int width, int height);
    void Render();

    
    // 内部渲染方法 (用于合成器回调)
    void BeginFrame();
    void CompositeForReadback();

    // 获取像素数据 (调用者负责 free)
    uint8_t* GetPixels() { return m_pixels; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    bool NeedsRender() const { return m_needsRender; }
    void ClearNeedsRender() { m_needsRender = false; }

    // 输入事件处理
    void HandleMouseMove(int x, int y);
    void HandleMouseDown(int x, int y, int button);
    void HandleMouseUp(int x, int y, int button);
    void HandleMouseWheel(int x, int y, int delta);
    void HandleKeyDown(int keyCode);
    void HandleKeyUp(int keyCode);
    void HandleChar(int charCode);

    // 关闭
    void Close();

private:
    // 内部初始化
    void InitPlatform();
    void InitWebView();

    // 渲染实现
    void DoRender();
    void ExtractPixels();
    
    // 辅助方法
    std::string EscapeJSString(const std::string& str);

    // 成员变量
    int m_width;
    int m_height;
    uint8_t* m_pixels;
    bool m_needsRender;
    bool m_initialized;
    int m_mouseButton;  // 当前按下的鼠标按钮 (0=无, 1=左, 2=中, 3=右)

    // Blink 对象 - 使用前向声明
    class SimplePlatform* m_platform;
    class LayerTreeViewImpl* m_layerTreeView;
    blink::WebView* m_webView;
};

}  // namespace html_viewer

#endif  // BLINK_WEB_WRAPPER_H_
