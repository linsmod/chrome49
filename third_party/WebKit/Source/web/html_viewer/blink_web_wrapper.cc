// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/html_viewer/blink_web_wrapper.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

// Blink public API headers
#include "public/platform/Platform.h"
#include "public/web/WebKit.h"  // for blink::initialize
#include "public/platform/WebClipboard.h"
#include "public/platform/WebCookieJar.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/WebData.h"
#include "public/platform/WebSize.h"
#include "public/platform/WebColor.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebString.h"
#include "public/platform/WebLayerTreeView.h"
#include "public/platform/WebViewScheduler.h"
#include "public/platform/WebFrameScheduler.h"
#include "public/platform/WebTaskRunner.h"
#include "public/web/WebInputEvent.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebViewClient.h"
#include "public/web/WebSettings.h"
#include "public/web/WebLocalFrame.h"
#include "public/web/WebTreeScopeType.h"
#include "public/web/WebFrame.h"
#include "public/platform/WebDragData.h"
#include "public/platform/WebImage.h"
#include "public/web/WebNode.h"
#include "public/web/WebWindowFeatures.h"
#include "public/web/WebFileChooserParams.h"
#include "public/web/WebDateTimeChooserParams.h"

// Blink internal headers
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
#include "platform/testing/TestingPlatformSupport.h"
#include "wtf/CurrentTime.h"
#include "wtf/OwnPtr.h"

// Skia headers
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImageInfo.h"

using namespace blink;

namespace html_viewer {

// ============================================================
// 第一部分: 自定义 Scheduler 实现
// TestingPlatformMockScheduler 的 createWebViewScheduler 返回 nullptr
// 我们需要提供自己的实现
// ============================================================

// 简单的 WebFrameScheduler 实现
class SimpleFrameScheduler : public WebFrameScheduler {
public:
    SimpleFrameScheduler() {}
    ~SimpleFrameScheduler() override {}
    
    WebTaskRunner* loadingTaskRunner() override { return nullptr; }
    WebTaskRunner* timerTaskRunner() override { return nullptr; }
};

// 简单的 WebViewScheduler 实现
class SimpleWebViewScheduler : public WebViewScheduler {
public:
    SimpleWebViewScheduler() {}
    ~SimpleWebViewScheduler() override {}
    
    void setPageInBackground(bool) override {}
    WebPassOwnPtr<WebFrameScheduler> createFrameScheduler() override {
        return adoptWebPtr(new SimpleFrameScheduler());
    }
};

// 自定义 WebScheduler，提供 createWebViewScheduler
class SimpleWebScheduler : public WebScheduler {
public:
    SimpleWebScheduler() : m_mockTaskRunner(adoptPtr(new SimpleTaskRunner())) {}
    ~SimpleWebScheduler() override {}
    
    WebTaskRunner* loadingTaskRunner() override { return m_mockTaskRunner.get(); }
    WebTaskRunner* timerTaskRunner() override { return m_mockTaskRunner.get(); }
    
    void shutdown() override {}
    bool shouldYieldForHighPriorityWork() override { return false; }
    bool canExceedIdleDeadlineIfRequired() override { return false; }
    void postIdleTask(const WebTraceLocation&, WebThread::IdleTask*) override {}
    void postNonNestableIdleTask(const WebTraceLocation&, WebThread::IdleTask*) override {}
    void postIdleTaskAfterWakeup(const WebTraceLocation&, WebThread::IdleTask*) override {}
    
    WebPassOwnPtr<WebViewScheduler> createWebViewScheduler(blink::WebView*) override {
        return adoptWebPtr(new SimpleWebViewScheduler());
    }
    
    void suspendTimerQueue() override {}
    void resumeTimerQueue() override {}
    void addPendingNavigation() override {}
    void removePendingNavigation() override {}
    void onNavigationStarted() override {}

private:
    // 简单的 TaskRunner 实现
    class SimpleTaskRunner : public WebTaskRunner {
    public:
        SimpleTaskRunner() {}
        ~SimpleTaskRunner() override {}
        
        void postTask(const WebTraceLocation&, Task* task) override {
            // 同步执行任务
            if (task) {
                task->run();
                delete task;
            }
        }
        
        void postDelayedTask(const WebTraceLocation&, Task* task, double) override {
            // 简化：同步执行
            postTask(WebTraceLocation(), task);
        }
        
        WebTaskRunner* clone() override { return new SimpleTaskRunner(); }
    };
    
    OwnPtr<SimpleTaskRunner> m_mockTaskRunner;
};

// 自定义 WebThread 实现
class SimpleWebThread : public WebThread {
public:
    SimpleWebThread() : m_scheduler(adoptPtr(new SimpleWebScheduler())) {}
    ~SimpleWebThread() override {}
    
    WebTaskRunner* taskRunner() override { return m_scheduler->timerTaskRunner(); }
    bool isCurrentThread() const override { return true; }
    WebScheduler* scheduler() const override { return m_scheduler.get(); }

private:
    OwnPtr<SimpleWebScheduler> m_scheduler;
};

// 自定义 Platform 实现
class SimplePlatform : public Platform {
public:
    SimplePlatform() : m_thread(adoptPtr(new SimpleWebThread())) {
        // 不在这里初始化 Platform，由 blink::initialize 完成
    }

    ~SimplePlatform() override {
        // Platform::shutdown 由 blink::shutdown 完成
    }
    
    WebThread* currentThread() override { return m_thread.get(); }
    
    // 时间函数
    double currentTimeSeconds() override { 
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        return ts.tv_sec + ts.tv_nsec / 1000000000.0;
    }
    
    double monotonicallyIncreasingTimeSeconds() override {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return ts.tv_sec + ts.tv_nsec / 1000000000.0;
    }
    
    WebString defaultLocale() override { return WebString::fromUTF8("en-US"); }
    
    // 其他必需方法返回空实现
    WebBlobRegistry* blobRegistry() override { return nullptr; }
    WebFileSystem* fileSystem() override { return nullptr; }
    WebIDBFactory* idbFactory() override { return nullptr; }
    WebScrollbarBehavior* scrollbarBehavior() override { return nullptr; }
    WebClipboard* clipboard() override { return nullptr; }
    WebFileUtilities* fileUtilities() override { return nullptr; }
    WebMimeRegistry* mimeRegistry() override { return nullptr; }
    WebThemeEngine* themeEngine() override { return nullptr; }
    WebURLLoader* createURLLoader() override { return nullptr; }
    WebCookieJar* cookieJar() override { return nullptr; }

private:
    OwnPtr<SimpleWebThread> m_thread;
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

    void layoutAndPaintAsync(WebLayoutAndPaintAsyncCallback*) override {
        // 同步执行
        if (m_renderer) m_renderer->Render();
    }

    void compositeAndReadbackAsync(WebCompositeAndReadbackAsyncCallback*) override {
        if (m_renderer) m_renderer->Render();
    }

    void setViewportSize(const WebSize& size) override { m_viewportSize = size; }
    void setDeviceScaleFactor(float) override {}
    void setBackgroundColor(WebColor color) override { m_backgroundColor = color; }
    void setHasTransparentBackground(bool) override {}

    WebSize getViewportSize() const { return m_viewportSize; }
    float getDeviceScaleFactor() const { return 1.0f; }
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

    // 1. 初始化 Platform - 使用自定义的 SimplePlatform
    // 提供完整的 Platform 实现，包括 WebThread 和 WebScheduler
    m_platform = new SimplePlatform();

    // 2. 初始化 Blink (必须传入有效的 Platform)
    blink::initialize(m_platform);

    // 3. 初始化 WebLayerTreeView
    m_layerTreeView = new LayerTreeViewImpl();
    m_layerTreeView->setRenderer(this);

    // 3. 设置测试环境 (参考 SimTest)
    LayoutTestSupport::setIsRunningLayoutTest(true);
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
    WebData data(html.data(), html.size());
    m_webView->mainFrame()->loadHTMLString(data, baseURL);

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
    // 创建 SkImageInfo 并分配像素
    SkImageInfo info = SkImageInfo::MakeN32Premul(m_width, m_height);
    SkBitmap bitmap;
    bitmap.allocPixels(info);
    
    SkCanvas canvas(bitmap);
    canvas.clear(SK_ColorWHITE);

    // 调用 WebView 绘制
    // m_webView->paint(&canvas, SkIntToScalar(m_width), SkIntToScalar(m_height));
    
    // 复制像素到输出缓冲区
    memcpy(m_pixels, bitmap.getPixels(), m_width * m_height * 4);
    
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
