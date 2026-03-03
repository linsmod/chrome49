// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/html_viewer/blink_web_wrapper.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

// Blink 头文件 - 使用相对于 web 目录的路径
#include "web/WebViewImpl.h"
#include "web/WebLocalFrameImpl.h"
#include "web/tests/FrameTestHelpers.h"

#include "core/frame/Settings.h"
#include "core/frame/FrameView.h"
#include "core/dom/Document.h"
#include "core/layout/LayoutView.h"
#include "core/paint/PaintLayer.h"
#include "platform/LayoutTestSupport.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/testing/URLTestHelpers.h"
#include "wtf/CurrentTime.h"

using namespace blink;

namespace html_viewer {

// ============================================================
// 第一部分: Platform 接口实现 (最小集)
// 基于 SimTest 测试框架
// ============================================================

class PlatformImpl : public Platform {
public:
    PlatformImpl() {}

    // 必须返回有效对象
    WebBlobRegistry* blobRegistry() override { return nullptr; }
    WebFileSystem* fileSystem() override { return nullptr; }
    WebIDBFactory* idbFactory() override { return nullptr; }
    WebScrollbarBehavior* scrollbarBehavior() override { return nullptr; }

    // 可选返回 nullptr
    WebClipboard* clipboard() override { return nullptr; }
    WebFileUtilities* fileUtilities() override { return nullptr; }
    WebMimeRegistry* mimeRegistry() override { return nullptr; }
    WebThemeEngine* themeEngine() override { return nullptr; }
    WebURLLoader* createURLLoader() override { return nullptr; }
    WebCookieJar* cookieJar() override { return nullptr; }

    // 时间函数 (必须返回有效值)
    double currentTimeSeconds() override { return (double)time(nullptr); }
    double monotonicallyIncreasingTimeSeconds() override {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return ts.tv_sec + ts.tv_nsec / 1000000000.0;
    }

    // 其他必需方法
    WebString defaultLocale() override { return WebString("en-US"); }
};

// ============================================================
// 第二部分: WebLayerTreeView 实现 (最小集)
// 参考 SimCompositor
// ============================================================

class LayerTreeViewImpl : public WebLayerTreeView {
public:
    LayerTreeViewImpl()
        : m_needsAnimate(false)
        , m_deferCommits(true)
        , m_viewportSize(1024, 768)
        , m_backgroundColor(0xFFFFFFFF)
        , m_renderer(nullptr) {}

    void setRenderer(BlinkWebRenderer* r) { m_renderer = r; }

    // WebLayerTreeView 虚函数实现
    void setNeedsAnimate() override {
        m_needsAnimate = true;
        if (m_renderer) m_renderer->ClearNeedsRender();
    }
    void setNeedsBeginFrame() override { setNeedsAnimate(); }
    void setNeedsCompositorUpdate() override {}
    void setDeferCommits(bool defer) override { m_deferCommits = defer; }

    void layoutAndPaintAsync(WebLayerTreeView::CompositorCallback*) override {
        // 同步执行
        if (m_renderer) m_renderer->Render();
    }

    void compositeAndReadbackAsync(WebLayerTreeView::CompositorCallback*) override {
        if (m_renderer) m_renderer->Render();
    }

    void setViewportSize(const WebSize& size) override { m_viewportSize = size; }
    void setDeviceScaleFactor(float) override {}
    void setBackgroundColor(WebColor color) override { m_backgroundColor = color; }
    void setHasTransparentBackground(bool) override {}
    void setSelectionBounds(const WebRect&, const WebRect&) override {}

    WebSize viewportSize() const override { return m_viewportSize; }
    float deviceScaleFactor() const override { return 1.0f; }
    bool needsAnimate() const { return m_needsAnimate; }
    bool deferCommits() const { return m_deferCommits; }

private:
    bool m_needsAnimate;
    bool m_deferCommits;
    WebSize m_viewportSize;
    WebColor m_backgroundColor;
    BlinkWebRenderer* m_renderer;
};

// ============================================================
// 第三部分: WebViewClient 实现 (最小集)
// 参考 SimWebViewClient
// ============================================================

class WebViewClientImpl : public WebViewClient {
public:
    explicit WebViewClientImpl(LayerTreeViewImpl* layerTree)
        : m_layerTree(layerTree) {}

    // 关键: 必须返回 layerTreeView
    WebLayerTreeView* layerTreeView() override { return m_layerTree; }

    // 其他方法返回空实现
    WebView* createView(WebLocalFrame*, const WebURLRequest&,
                       const WebWindowFeatures&, const WebString&,
                       WebNavigationPolicy, bool) override {
        return nullptr;
    }
    
    void printPage(WebLocalFrame*) override {}
    bool runFileChooser(const WebFileChooserParams&, WebFileChooserCompletion*) override {
        return false;
    }
    bool openDateTimeChooser(const WebDateTimeChooserParams&,
                           WebDateTimeChooserCompletion*) override {
        return false;
    }
    void startDragging(WebLocalFrame*, const WebDragData&,
                      WebDragOperationsMask, const WebImage&,
                      const WebPoint&) override {}
    void focusedNodeChanged(const WebNode&, const WebNode&) override {}

private:
    LayerTreeViewImpl* m_layerTree;
};

// ============================================================
// 第四部分: WebFrameClient 实现 (最小集)
// ============================================================

class WebFrameClientImpl : public WebFrameClient {
public:
    WebFrameClientImpl() {}
    
    WebFrame* createChildFrame(WebLocalFrame* parent, WebTreeScopeType,
                              const WebString& frameName, WebSandboxFlags,
                              const WebFrameOwnerProperties&) override {
        return nullptr;
    }
    
    void frameDetached(WebFrame*, DetachType) override {}
    void didStartLoading(bool) override {}
    void didStopLoading() override {}
};

// ============================================================
// 第五部分: BlinkWebRenderer 实现
// ============================================================

BlinkWebRenderer::BlinkWebRenderer(int width, int height)
    : m_width(width)
    , m_height(height)
    , m_pixels(nullptr)
    , m_needsRender(true)
    , m_initialized(false)
    , m_platform(nullptr)
    , m_layerTreeView(nullptr)
    , m_webView(nullptr)
{
}

BlinkWebRenderer::~BlinkWebRenderer() {
    Close();
}

bool BlinkWebRenderer::Initialize() {
    if (m_initialized)
        return true;

    // 1. 初始化 Platform
    m_platform = new PlatformImpl();

    // 2. 初始化 WebLayerTreeView
    m_layerTreeView = new LayerTreeViewImpl();
    m_layerTreeView->setRenderer(this);

    // 3. 设置测试环境 (参考 SimTest)
    Document::setThreadedParsingEnabledForTesting(false);
    LayoutTestSupport::setMockThemeEnabledForTest(true);
    ScrollbarTheme::setMockScrollbarsEnabled(true);
    FrameView::setInitialTracksPaintInvalidationsForTesting(true);
    GraphicsLayer::setDrawDebugRedFillForTesting(false);

    // 4. 创建 WebView
    // 使用 FrameTestHelpers::WebViewHelper 方式
    WebViewClientImpl* viewClient = new WebViewClientImpl(m_layerTreeView);
    WebFrameClientImpl* frameClient = new WebFrameClientImpl();
    
    m_webView = WebViewImpl::create(viewClient);
    WebLocalFrameImpl* frame = WebLocalFrameImpl::create(
        WebTreeScopeType::Document, frameClient);
    m_webView->setMainFrame(frame);
    
    // 5. 设置视口大小
    m_webView->resize(WebSize(m_width, m_height));
    m_layerTreeView->setViewportSize(WebSize(m_width, m_height));

    // 6. 启用 JavaScript
    WebSettings* settings = m_webView->settings();
    settings->setJavaScriptEnabled(true);
    settings->setLoadsImagesAutomatically(true);

    // 7. 分配像素缓冲区
    m_pixels = (uint8_t*)malloc(m_width * m_height * 4);  // RGBA

    m_initialized = true;
    return true;
}

void BlinkWebRenderer::LoadHTML(const std::string& html) {
    if (!m_webView || !m_initialized)
        return;

    WebURL baseURL = URLTestHelpers::toKURL("http://example.com/");
    FrameTestHelpers::loadHTMLString(
        m_webView->mainFrame(),
        html,
        baseURL
    );

    // 标记需要渲染
    m_needsRender = true;
}

void BlinkWebRenderer::LoadURL(const std::string& url) {
    if (!m_webView || !m_initialized)
        return;

    WebURLRequest request;
    request.initialize();
    request.setURL(KURL(ParsedURLString, url.c_str()));
    m_webView->mainFrameImpl()->loadRequest(request);

    m_needsRender = true;
}

void BlinkWebRenderer::Resize(int width, int height) {
    if (!m_webView || !m_initialized)
        return;

    m_width = width;
    m_height = height;
    
    m_webView->resize(WebSize(width, height));
    m_layerTreeView->setViewportSize(WebSize(width, height));

    // 重新分配像素缓冲区
    if (m_pixels) free(m_pixels);
    m_pixels = (uint8_t*)malloc(width * height * 4);

    m_needsRender = true;
}

void BlinkWebRenderer::Render() {
    if (!m_webView || !m_initialized || !m_needsRender)
        return;

    DoRender();
}

void BlinkWebRenderer::DoRender() {
    if (!m_webView)
        return;

    // 参考 SimCompositor::beginFrame()
    double lastFrameTime = monotonicallyIncreasingTime() + 0.016;
    
    m_webView->beginFrame(lastFrameTime);
    m_webView->updateAllLifecyclePhases();

    // 提取像素
    ExtractPixels();
    
    m_needsRender = false;
}

void BlinkWebRenderer::ExtractPixels() {
    if (!m_pixels || !m_webView)
        return;

    // 使用 Skia 获取像素
    // 注意: 这里需要通过 SkCanvas 进行绘制
    // 简化版本: 创建 SkBitmap
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, m_width, m_height);
    bitmap.allocPixels();
    bitmap.setPixels(m_pixels);

    SkCanvas canvas(bitmap);
    canvas.clear(SK_ColorWHITE);

    // 调用 WebView 绘制
    // m_webView->paint(&canvas, SkIntToScalar(m_width), SkIntToScalar(m_height));
    
    // 暂时使用空白，实际需要调用 paint 方法
    // TODO: 实现完整的 paint 调用
}

void BlinkWebRenderer::HandleMouseMove(int x, int y) {
    if (!m_webView)
        return;

    WebMouseEvent event;
    event.type = WebInputEvent::MouseMove;
    event.x = x;
    event.y = y;
    event.windowX = x;
    event.windowY = y;
    event.clickCount = 0;
    
    m_webView->handleInputEvent(event);
}

void BlinkWebRenderer::HandleMouseDown(int x, int y, int button) {
    if (!m_webView)
        return;

    WebMouseEvent event;
    event.type = WebInputEvent::MouseDown;
    event.x = x;
    event.y = y;
    event.windowX = x;
    event.windowY = y;
    event.button = static_cast<WebMouseEvent::Button>(button);
    event.clickCount = 1;
    
    m_webView->handleInputEvent(event);
    m_needsRender = true;
}

void BlinkWebRenderer::HandleMouseUp(int x, int y, int button) {
    if (!m_webView)
        return;

    WebMouseEvent event;
    event.type = WebInputEvent::MouseUp;
    event.x = x;
    event.y = y;
    event.windowX = x;
    event.windowY = y;
    event.button = static_cast<WebMouseEvent::Button>(button);
    event.clickCount = 1;
    
    m_webView->handleInputEvent(event);
}

void BlinkWebRenderer::HandleMouseWheel(int x, int y, int delta) {
    if (!m_webView)
        return;

    WebMouseWheelEvent event;
    event.type = WebInputEvent::MouseWheel;
    event.x = x;
    event.y = y;
    event.windowX = x;
    event.windowY = y;
    event.deltaY = delta;
    
    m_webView->handleInputEvent(event);
    m_needsRender = true;
}

void BlinkWebRenderer::HandleKeyDown(int keyCode) {
    if (!m_webView)
        return;

    WebKeyboardEvent event;
    event.type = WebInputEvent::RawKeyDown;
    event.windowsKeyCode = keyCode;
    
    m_webView->handleInputEvent(event);
}

void BlinkWebRenderer::HandleKeyUp(int keyCode) {
    if (!m_webView)
        return;

    WebKeyboardEvent event;
    event.type = WebInputEvent::KeyUp;
    event.windowsKeyCode = keyCode;
    
    m_webView->handleInputEvent(event);
}

void BlinkWebRenderer::HandleKeyPress(int keyCode) {
    if (!m_webView)
        return;

    WebKeyboardEvent event;
    event.type = WebInputEvent::Char;
    event.windowsKeyCode = keyCode;
    
    m_webView->handleInputEvent(event);
    m_needsRender = true;
}

void BlinkWebRenderer::Close() {
    if (!m_initialized)
        return;

    // 清理测试环境
    Document::setThreadedParsingEnabledForTesting(true);
    LayoutTestSupport::setMockThemeEnabledForTest(false);
    ScrollbarTheme::setMockScrollbarsEnabled(false);
    FrameView::setInitialTracksPaintInvalidationsForTesting(false);
    GraphicsLayer::setDrawDebugRedFillForTesting(true);

    // 关闭 WebView
    if (m_webView) {
        m_webView->close();
        m_webView = nullptr;
    }

    // 释放内存
    if (m_pixels) {
        free(m_pixels);
        m_pixels = nullptr;
    }

    delete m_layerTreeView;
    delete m_platform;

    m_initialized = false;
}

// ============================================================
// 全局初始化/关闭
// ============================================================

static BlinkWebConfig* g_config = nullptr;

void InitializeBlinkWeb(const BlinkWebConfig* config) {
    g_config = new BlinkWebConfig();
    if (config) {
        *g_config = *config;
    }
}

void ShutdownBlinkWeb() {
    delete g_config;
    g_config = nullptr;
}

}  // namespace html_viewer
