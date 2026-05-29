// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/simpleblink/blink_web_wrapper.h"
#include "web/simpleblink/web_url_loader_curl.h"
#include <curl/curl.h>
#include <cstdio>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <map>

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
#include "public/platform/WebURLLoader.h"
#include "public/platform/WebURLLoaderClient.h"
#include "public/platform/WebURLResponse.h"
#include "public/web/WebInputEvent.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebViewClient.h"
#include "public/web/WebSettings.h"
#include "public/web/WebLocalFrame.h"
#include "public/web/WebTreeScopeType.h"
#include "public/web/WebLocalFrame.h"
#include "public/platform/WebDragData.h"
#include "public/platform/WebMimeRegistry.h"
#include "public/platform/WebImage.h"
#include "public/platform/WebStorageArea.h"
#include "public/platform/WebStorageNamespace.h"
#include "public/platform/WebBlobRegistry.h"
#include "public/platform/WebURL.h"
#include "public/web/WebView.h"
#include "public/web/WebFrame.h"
#include "public/web/WebNode.h"
#include "public/web/WebWindowFeatures.h"
#include "public/web/WebFileChooserParams.h"
#include "public/web/WebDateTimeChooserParams.h"
#include "public/web/WebScriptSource.h"

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
#include "platform/EventTracer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "web_theme_engine_impl.h"
#include "wtf/CurrentTime.h"
#include "wtf/OwnPtr.h"

// Skia headers
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImageInfo.h"

// Resource headers
#include "blink/public/resources/grit/blink_resources.h"
#include "web/simpleblink/pak_resource.h"

// Compositor headers
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebContentLayer.h"
#include "public/platform/WebLayer.h"
#include "public/platform/WebContentLayerClient.h"
// CC Blink 
#include "cc/blink/web_compositor_support_impl.h"
#include "web/simpleblink/simple_web_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
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
    explicit SimpleFrameScheduler(WebTaskRunner* runner) : m_taskRunner(runner) {}
    ~SimpleFrameScheduler() override {}
    
    WebTaskRunner* loadingTaskRunner() override { return m_taskRunner; }
    WebTaskRunner* timerTaskRunner() override { return m_taskRunner; }

private:
    WebTaskRunner* m_taskRunner;
};

// 简单的 WebViewScheduler 实现
class SimpleWebViewScheduler : public WebViewScheduler {
public:
    explicit SimpleWebViewScheduler(WebTaskRunner* runner) : m_taskRunner(runner) {}
    ~SimpleWebViewScheduler() override {}
    
    void setPageInBackground(bool) override {}
    WebPassOwnPtr<WebFrameScheduler> createFrameScheduler() override {
        return adoptWebPtr(new SimpleFrameScheduler(m_taskRunner));
    }

private:
    WebTaskRunner* m_taskRunner;
};

// 自定义 WebScheduler，提供 createWebViewScheduler
class SimpleWebScheduler : public WebScheduler {
public:
    SimpleWebScheduler() 
        : m_taskRunner(adoptPtr(new SimpleWebTaskRunner(
              base::ThreadTaskRunnerHandle::Get()))) {}
    ~SimpleWebScheduler() override {}
    
    WebTaskRunner* loadingTaskRunner() override { return m_taskRunner.get(); }
    WebTaskRunner* timerTaskRunner() override { return m_taskRunner.get(); }
    
    void shutdown() override {}
    bool shouldYieldForHighPriorityWork() override { return false; }
    bool canExceedIdleDeadlineIfRequired() override { return false; }
    void postIdleTask(const WebTraceLocation&, WebThread::IdleTask*) override {}
    void postNonNestableIdleTask(const WebTraceLocation&, WebThread::IdleTask*) override {}
    void postIdleTaskAfterWakeup(const WebTraceLocation&, WebThread::IdleTask*) override {}
    
    WebPassOwnPtr<WebViewScheduler> createWebViewScheduler(blink::WebView*) override {
        return adoptWebPtr(new SimpleWebViewScheduler(m_taskRunner.get()));
    }
    
    void suspendTimerQueue() override {}
    void resumeTimerQueue() override {}
    void addPendingNavigation() override {}
    void removePendingNavigation() override {}
    void onNavigationStarted() override {}

private:
    OwnPtr<SimpleWebTaskRunner> m_taskRunner;
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

// ============================================================
// Worker thread scheduler - minimal stub for background threads.
// ============================================================

class SimpleWorkerScheduler : public WebScheduler {
public:
    explicit SimpleWorkerScheduler(WebTaskRunner* runner) : m_taskRunner(runner) {}
    ~SimpleWorkerScheduler() override {}

    void shutdown() override {}
    bool shouldYieldForHighPriorityWork() override { return false; }
    bool canExceedIdleDeadlineIfRequired() override { return false; }
    void postIdleTask(const WebTraceLocation&, WebThread::IdleTask*) override {}
    void postNonNestableIdleTask(const WebTraceLocation&, WebThread::IdleTask*) override {}
    void postIdleTaskAfterWakeup(const WebTraceLocation&, WebThread::IdleTask*) override {}
    WebTaskRunner* loadingTaskRunner() override { return m_taskRunner; }
    WebTaskRunner* timerTaskRunner() override { return m_taskRunner; }
    WebPassOwnPtr<WebViewScheduler> createWebViewScheduler(WebView*) override {
        return adoptWebPtr(static_cast<WebViewScheduler*>(nullptr));
    }
    void suspendTimerQueue() override {}
    void resumeTimerQueue() override {}
    void addPendingNavigation() override {}
    void removePendingNavigation() override {}
    void onNavigationStarted() override {}

private:
    WebTaskRunner* m_taskRunner;
};

// Worker thread wrapping base::Thread for background tasks (e.g. ScriptStreamerThread).
class SimpleWorkerThread : public WebThread {
public:
    explicit SimpleWorkerThread(const char* name) {
        m_baseThread.reset(new base::Thread(name ? name : "SimpleWorker"));
        bool started = m_baseThread->Start();
        CHECK(started);
        m_taskRunner = adoptPtr(new SimpleWebTaskRunner(
            m_baseThread->task_runner()));
        m_scheduler = adoptPtr(new SimpleWorkerScheduler(m_taskRunner.get()));
        m_threadId = m_baseThread->GetThreadId();
    }
    ~SimpleWorkerThread() override {
        if (m_baseThread)
            m_baseThread->Stop();
    }

    WebTaskRunner* taskRunner() override { return m_taskRunner.get(); }
    bool isCurrentThread() const override {
        return m_threadId == base::PlatformThread::CurrentId();
    }
    PlatformThreadId threadId() const override {
        return static_cast<PlatformThreadId>(m_threadId);
    }
    WebScheduler* scheduler() const override { return m_scheduler.get(); }

private:
    scoped_ptr<base::Thread> m_baseThread;
    OwnPtr<SimpleWebTaskRunner> m_taskRunner;
    OwnPtr<SimpleWorkerScheduler> m_scheduler;
    base::PlatformThreadId m_threadId;
};

// ============================================================
// Simple storage - in-memory implementation for session/localStorage
// ============================================================

class SimpleWebStorageArea : public WebStorageArea {
public:
    unsigned length() override { return m_values.size(); }
    WebString key(unsigned index) override {
        if (index >= m_values.size())
            return WebString();
        auto it = m_values.begin();
        std::advance(it, index);
        return WebString::fromUTF8(it->first);
    }
    WebString getItem(const WebString& key) override {
        auto it = m_values.find(key.utf8());
        if (it == m_values.end())
            return WebString();
        return WebString::fromUTF8(it->second);
    }
    void setItem(const WebString& key, const WebString& newValue,
                 const WebURL&, Result& result, WebString&) override {
        m_values[key.utf8()] = newValue.utf8();
        result = ResultOK;
    }
    void removeItem(const WebString& key, const WebURL&, WebString&) override {
        m_values.erase(key.utf8());
    }
    void clear(const WebURL&, bool&) override {
        m_values.clear();
    }
private:
    std::map<std::string, std::string> m_values;
};

class SimpleWebStorageNamespace : public WebStorageNamespace {
public:
    WebStorageArea* createStorageArea(const WebString&) override {
        return new SimpleWebStorageArea();
    }
};

// ============================================================
// Simple Blob Registry - no-op in-memory implementation
// ============================================================

class SimpleWebBlobRegistry : public WebBlobRegistry {
public:
    Builder* createBuilder(const WebString& uuid, const WebString& contentType) override {
        return nullptr;
    }
};

// ============================================================
// MIME Registry - 告知 Blink 哪些 MIME 类型可以内联显示
// ============================================================

class SimpleMimeRegistry : public WebMimeRegistry {
public:
    SupportsType supportsMIMEType(const WebString& mimeType) override {
        if (mimeType == "text/html" || mimeType == "text/plain" ||
            mimeType == "text/css" || mimeType == "application/javascript" ||
            mimeType == "application/x-javascript" || mimeType == "text/javascript" ||
            mimeType == "image/png" || mimeType == "image/jpeg" ||
            mimeType == "image/gif" || mimeType == "image/svg+xml" ||
            mimeType == "image/webp" || mimeType == "image/x-icon" ||
            mimeType == "application/json" || mimeType == "application/xml" ||
            mimeType == "text/xml" || mimeType == "application/pdf")
            return IsSupported;
        return IsNotSupported;
    }

    SupportsType supportsImageMIMEType(const WebString& mimeType) override {
        if (mimeType == "image/png" || mimeType == "image/jpeg" ||
            mimeType == "image/gif" || mimeType == "image/svg+xml" ||
            mimeType == "image/webp" || mimeType == "image/x-icon")
            return IsSupported;
        return IsNotSupported;
    }

    SupportsType supportsImagePrefixedMIMEType(const WebString&) override {
        return IsNotSupported;
    }

    SupportsType supportsJavaScriptMIMEType(const WebString& mimeType) override {
        if (mimeType == "application/javascript" || mimeType == "application/x-javascript" ||
            mimeType == "text/javascript" || mimeType == "text/ecmascript")
            return IsSupported;
        return IsNotSupported;
    }

    SupportsType supportsMediaMIMEType(const WebString&,
        const WebString&, const WebString&) override {
        return IsNotSupported;
    }

    bool supportsMediaSourceMIMEType(const WebString&,
        const WebString&) override {
        return false;
    }

    SupportsType supportsNonImageMIMEType(const WebString& mimeType) override {
        return supportsMIMEType(mimeType);
    }

    WebString mimeTypeForExtension(const WebString&) override {
        return WebString();
    }

    WebString wellKnownMimeTypeForExtension(const WebString&) override {
        return WebString();
    }
};

// 自定义 Platform 实现
class SimplePlatform : public Platform {
public:
    SimplePlatform() 
        : m_thread(adoptPtr(new SimpleWebThread())) 
        , m_compositorSupport(adoptPtr(new cc_blink::WebCompositorSupportImpl())){
        // 不在这里初始化 Platform，由 blink::initialize 完成
    }

    ~SimplePlatform() override {
        // Platform::shutdown 由 blink::shutdown 完成
    }
    
    WebThread* currentThread() override { return m_thread.get(); }
    
    WebCompositorSupport* compositorSupport() override {
        return m_compositorSupport.get();
    }
    
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
    
    // User Agent - 必需！
    WebString userAgent() override {
        return WebString::fromUTF8("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/49.0.0.0 Safari/537.36");
    }
    
    // Tracing 支持 - 返回禁用状态
    const unsigned char* getTraceCategoryEnabledFlag(const char* categoryName) override {
        static const unsigned char tracingIsDisabled = 0;
        return &tracingIsDisabled;
    }
    
    // 资源加载 - 从 PAK 文件加载 Blink 资源
    WebData loadResource(const char* name) override {
        // 资源名称到 IDR 的映射
        static const struct {
            const char* name;
            uint16_t id;
        } kResources[] = {
            {"html.css", IDR_UASTYLE_HTML_CSS},
            {"quirks.css", IDR_UASTYLE_QUIRKS_CSS},
            {"view-source.css", IDR_UASTYLE_VIEW_SOURCE_CSS},
            {"svg.css", IDR_UASTYLE_SVG_CSS},
            {"mathml.css", IDR_UASTYLE_MATHML_CSS},
            {"mediaControls.css", IDR_UASTYLE_MEDIA_CONTROLS_CSS},
            {"fullscreen.css", IDR_UASTYLE_FULLSCREEN_CSS},
            {"xhtmlmp.css", IDR_UASTYLE_XHTMLMP_CSS},
            {"themeWin.css", IDR_UASTYLE_THEME_WIN_CSS},
            {"themeWinQuirks.css", IDR_UASTYLE_THEME_WIN_QUIRKS_CSS},
            {"themeChromiumLinux.css", IDR_UASTYLE_THEME_CHROMIUM_LINUX_CSS},
            {"themeInputMultipleFields.css", IDR_UASTYLE_THEME_INPUT_MULTIPLE_FIELDS_CSS},
        };
        
        // 懒加载 PAK 资源
        static PakResource* s_pakResource = nullptr;
        if (!s_pakResource) {
            s_pakResource = new PakResource();
            // 加载 blink_resources.pak
            const char* pakPath = "/home/wulin/chrome49/src/out/Release/gen/blink/public/resources/blink_resources.pak";
            if (!s_pakResource->LoadFromFile(pakPath)) {
                fprintf(stderr, "Failed to load blink_resources.pak\n");
                return WebData();
            }
        }
        
        for (size_t i = 0; i < sizeof(kResources) / sizeof(kResources[0]); ++i) {
            if (!strcmp(name, kResources[i].name)) {
                const char* data = nullptr;
                size_t size = 0;
                if (s_pakResource->GetResource(kResources[i].id, &data, &size)) {
                    return WebData(data, size);
                }
                break;
            }
        }
        
        // 资源未找到
        fprintf(stderr, "Warning: Resource not found: %s\n", name);
        return WebData();
    }
    
    // URL Loader - 基于 libcurl 实现
    WebURLLoader* createURLLoader() override {
        return new WebURLLoaderCurl();
    }

    WebThread* createThread(const char* name) override {
        return new SimpleWorkerThread(name);
    }
    
    // 其他必需方法返回空实现
    WebBlobRegistry* blobRegistry() override { return &m_blobRegistry; }
    WebFileSystem* fileSystem() override { return nullptr; }
    WebIDBFactory* idbFactory() override { return nullptr; }
    WebScrollbarBehavior* scrollbarBehavior() override { return nullptr; }
    WebClipboard* clipboard() override { return nullptr; }
    WebFileUtilities* fileUtilities() override { return nullptr; }
    WebMimeRegistry* mimeRegistry() override { return &m_mimeRegistry; }
    WebThemeEngine* themeEngine() override { return &theme_engine_; }
    WebCookieJar* cookieJar() override { return nullptr; }
    WebStorageNamespace* createLocalStorageNamespace() override {
        return new SimpleWebStorageNamespace();
    }

private:
    OwnPtr<SimpleWebThread> m_thread;
    OwnPtr<cc_blink::WebCompositorSupportImpl> m_compositorSupport;
    SimpleMimeRegistry m_mimeRegistry;
    SimpleWebBlobRegistry m_blobRegistry;
    WebThemeEngineImpl theme_engine_;
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

class WebViewClientImpl : public WebViewClient {
public:
    explicit WebViewClientImpl(LayerTreeViewImpl* layerTree)
        : m_layerTree(layerTree) {}

    // 关键: 必须返回 layerTreeView
    WebLayerTreeView* layerTreeView() override { return m_layerTree; }
    
    // 允许 null layerTreeView - 软件渲染模式必需
    bool allowsBrokenNullLayerTreeView() const override { return true; }

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
    WebStorageNamespace* createSessionStorageNamespace() override {
        return new SimpleWebStorageNamespace();
    }

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
    , m_mouseButton(0)
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

    // 0. 全局初始化 libcurl (线程安全，在创建后台线程之前调用)
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // 1. 初始化 Platform - 使用自定义的 SimplePlatform
    m_platform = new SimplePlatform();

    // 2. 初始化 Blink (必须传入有效的 Platform)
    blink::initialize(m_platform);

    // 2.5 初始化 EventTracer (用于 tracing 支持)
    blink::EventTracer::initialize();

    // 3. 设置测试环境 (参考 SimTest)
    // LayoutTestSupport::setIsRunningLayoutTest(true);
    Document::setThreadedParsingEnabledForTesting(false);
    // LayoutTestSupport::setMockThemeEnabledForTest(true);
    // ScrollbarTheme::setMockScrollbarsEnabled(true);
    // FrameView::setInitialTracksPaintInvalidationsForTesting(true);
    GraphicsLayer::setDrawDebugRedFillForTesting(false);

    // 4. 创建 LayerTreeView (用于软件渲染)
    m_layerTreeView = new LayerTreeViewImpl();
    m_layerTreeView->setRenderer(this);
    
    // 5. 创建 WebView (软件渲染模式)
    WebViewClientImpl* viewClient = new WebViewClientImpl(m_layerTreeView);
    WebFrameClientImpl* frameClient = new WebFrameClientImpl();
    
    m_webView = WebView::create(viewClient);
    WebLocalFrame* frame = WebLocalFrame::create(
        WebTreeScopeType::Document, frameClient);
    m_webView->setMainFrame(frame);
    
    // 6. 关键: 先禁用加速合成模式，再 resize
    // 这是软件渲染的关键设置，必须在 resize 之前调用
    WebSettings* settings = m_webView->settings();
    settings->setAcceleratedCompositingEnabled(false);  // 禁用合成层
    settings->setJavaScriptEnabled(true);
    settings->setLoadsImagesAutomatically(true);
    
    // 7. 设置视口大小 (在禁用合成之后)
    m_webView->resize(WebSize(m_width, m_height));

    // 8. 激活 WebView，使鼠标点击可以聚焦输入元素
    m_webView->setFocus(true);

    // 9. 分配像素缓冲区
    m_pixels = (uint8_t*)malloc(m_width * m_height * 4);  // RGBA

    m_initialized = true;
    return true;
}

void BlinkWebRenderer::LoadHTML(const std::string& html) {
    if (!m_webView || !m_initialized)
        return;

    WebFrame* frame = m_webView->mainFrame();
    if (!frame)
        return;

    // 方法1: 使用 JavaScript document.write() 直接写入 HTML
    // 这是一种同步方式，不需要异步加载
    WebScriptSource source(
        WebString::fromUTF8("document.open(); document.write('" + 
            EscapeJSString(html) + "'); document.close();")
    );
    frame->executeScript(source);

    // 标记需要渲染
    m_needsRender = true;
}

// 辅助函数: 转义 JavaScript 字符串
std::string BlinkWebRenderer::EscapeJSString(const std::string& str) {
    std::string result;
    result.reserve(str.size() * 2);
    for (char c : str) {
        switch (c) {
            case '\'': result += "\\'"; break;
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c; break;
        }
    }
    return result;
}

void BlinkWebRenderer::LoadURL(const std::string& url) {
    if (!m_webView || !m_initialized)
        return;

    WebURLRequest request;
    request.initialize();
    request.setURL(KURL(ParsedURLString, url.c_str()));
    m_webView->mainFrame()->loadRequest(request);

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
    if (!m_webView || !m_initialized)
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

    // 软件渲染: 使用 WebViewImpl::paint() 直接绘制到 SkCanvas
    // 注意: paint() 只能在合成未激活时使用 (setAcceleratedCompositingEnabled(false))
    
    // 创建 Skia 位图和画布，使用非预乘格式（与 SDL 一致）
    SkImageInfo info = SkImageInfo::MakeN32(m_width, m_height, kUnpremul_SkAlphaType);
    SkBitmap bitmap;
    bitmap.allocPixels(info);
    
    SkCanvas canvas(bitmap);
    canvas.clear(SK_ColorWHITE);

    // 使用 WebView 的软件渲染 paint 方法
    // 这会调用 PageWidgetDelegate::paint() 进行软件绘制
    WebRect rect(0, 0, m_width, m_height);
    m_webView->paint(&canvas, rect);
    
    // 复制像素到输出缓冲区
    // Skia 输出已是非预乘格式（BGRA 字节序 = SDL ARGB8888）
    const uint8_t* srcPixels = static_cast<const uint8_t*>(bitmap.getPixels());
    memcpy(m_pixels, srcPixels, m_width * m_height * 4);
    
    // if (!hasContent) {
    //     fprintf(stderr, "Warning: Rendered image is all white - no content rendered\n");
    // } else {
    //     fprintf(stderr, "Rendered image has content\n");
    // }
    
    // 保存渲染结果到文件用于调试
    // FILE* f = fopen("/tmp/blink_render.raw", "wb");
    // if (f) {
    //     fwrite(m_pixels, 1, m_width * m_height * 4, f);
    //     fclose(f);
    //     fprintf(stderr, "Saved raw pixel data to /tmp/blink_render.raw (%dx%d RGBA)\n", m_width, m_height);
    // }
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
    event.globalX = x;
    event.globalY = y;
    event.clickCount = 0;
    
    // 设置当前按下的按钮 (用于拖动)
    switch (m_mouseButton) {
        case 1:
            event.button = WebMouseEvent::ButtonLeft;
            event.modifiers = WebInputEvent::LeftButtonDown;
            break;
        case 2:
            event.button = WebMouseEvent::ButtonMiddle;
            event.modifiers = WebInputEvent::MiddleButtonDown;
            break;
        case 3:
            event.button = WebMouseEvent::ButtonRight;
            event.modifiers = WebInputEvent::RightButtonDown;
            break;
        default:
            event.button = WebMouseEvent::ButtonLeft;
            event.modifiers = 0;
            break;
    }
    
    m_webView->handleInputEvent(event);
    m_needsRender = true;
}

void BlinkWebRenderer::HandleMouseDown(int x, int y, int button) {
    if (!m_webView)
        return;

    blink::WebMouseEvent event;
    event.type = blink::WebInputEvent::MouseDown;
    event.x = x;
    event.y = y;
    event.windowX = x;
    event.windowY = y;
    event.globalX = x;
    event.globalY = y;
    event.modifiers = 0;
    event.clickCount = 1;
    
    // 转换 SDL 按钮值到 Blink 按钮值
    switch (button) {
        case 1:  // SDL_BUTTON_LEFT
            event.button = blink::WebMouseEvent::ButtonLeft;
            break;
        case 2:  // SDL_BUTTON_MIDDLE
            event.button = blink::WebMouseEvent::ButtonMiddle;
            break;
        case 3:  // SDL_BUTTON_RIGHT
            event.button = blink::WebMouseEvent::ButtonRight;
            break;
        // SDL_BUTTON_X1 (4) 和 SDL_BUTTON_X2 (5) 是额外按钮
        case 4:
        case 5:
            return;  // ignore X1/X2 buttons
        default:
            event.button = blink::WebMouseEvent::ButtonLeft;
            break;
    }
    
    m_webView->handleInputEvent(event);
    m_mouseButton = button;  // 记录按下的按钮
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
    event.globalX = x;
    event.globalY = y;
    event.clickCount = 1;
    
    // 转换 SDL 按钮值到 Blink 按钮值
    switch (button) {
        case 1:
            event.button = WebMouseEvent::ButtonLeft;
            break;
        case 2:
            event.button = WebMouseEvent::ButtonMiddle;
            break;
        case 3:
            event.button = WebMouseEvent::ButtonRight;
            break;
        default:
            event.button = WebMouseEvent::ButtonLeft;
            break;
    }
    
    m_webView->handleInputEvent(event);
    m_mouseButton = 0;  // 清除按钮状态
    m_needsRender = true;
}

void BlinkWebRenderer::HandleMouseWheel(int x, int y, int delta) {
    if (!m_webView)
        return;

    WebMouseWheelEvent event;
    event.type = WebInputEvent::MouseWheel;
    
    event.timeStampSeconds = monotonicallyIncreasingTime();
    
    event.x = x;
    event.y = y;
    event.windowX = x;
    event.windowY = y;
    event.globalX = x;
    event.globalY = y;
    
    // SDL wheel.y > 0 = scroll up, Blink deltaY > 0 = scroll down → negate
    event.deltaX = 0;
    event.deltaY = delta * 120.0f;
    
    event.wheelTicksX = 0;
    event.wheelTicksY = delta;
    
    event.hasPreciseScrollingDeltas = false;
    event.canScroll = true;
    event.button = WebMouseEvent::ButtonNone;
    
    event.modifiers = 0;
    if (m_mouseButton == 1)
        event.modifiers |= WebInputEvent::LeftButtonDown;
    else if (m_mouseButton == 2)
        event.modifiers |= WebInputEvent::MiddleButtonDown;
    else if (m_mouseButton == 3)
        event.modifiers |= WebInputEvent::RightButtonDown;
    
    event.phase = WebMouseWheelEvent::PhaseChanged;
    event.momentumPhase = WebMouseWheelEvent::PhaseNone;
    
    m_webView->handleInputEvent(event);
    m_needsRender = true;
}

// SDL keycode → Windows VK code mapping
static int sdlToWindowsVK(int sym) {
    // Letters: SDL uses ASCII (a=97), VK expects uppercase (VK_A=65)
    if (sym >= 'a' && sym <= 'z')
        return sym - 32;
    // Numbers: SDL ASCII matches VK
    if (sym >= '0' && sym <= '9')
        return sym;
    // Special keys where SDL sym matches VK
    if (sym == '\r') return 13;      // VK_RETURN
    if (sym == '\033') return 27;    // VK_ESCAPE
    if (sym == '\b') return 8;       // VK_BACK
    if (sym == '\t') return 9;       // VK_TAB
    if (sym == ' ') return 32;       // VK_SPACE
    if (sym == '\177') return 46;    // VK_DELETE
    // Arrow/cursor keys (SDL uses high scancode values)
    if (sym == 1073741906) return 38;   // VK_UP
    if (sym == 1073741905) return 40;   // VK_DOWN
    if (sym == 1073741904) return 37;   // VK_LEFT
    if (sym == 1073741903) return 39;   // VK_RIGHT
    if (sym == 1073741898) return 36;   // VK_HOME
    if (sym == 1073741901) return 35;   // VK_END
    if (sym == 1073741899) return 33;   // VK_PRIOR (PageUp)
    if (sym == 1073741902) return 34;   // VK_NEXT (PageDown)
    if (sym == 1073741897) return 45;   // VK_INSERT
    // Function keys F1-F12
    if (sym >= 1073741882 && sym <= 1073741893)
        return 112 + (sym - 1073741882); // VK_F1=112
    return sym; // fallback — pass through
}

void BlinkWebRenderer::HandleKeyDown(int keyCode) {
    if (!m_webView)
        return;

    WebKeyboardEvent event;
    event.type = WebInputEvent::RawKeyDown;
    event.windowsKeyCode = sdlToWindowsVK(keyCode);
    event.nativeKeyCode = keyCode;
    event.modifiers = 0;
    event.text[0] = 0;
    event.unmodifiedText[0] = 0;
    event.setKeyIdentifierFromWindowsKeyCode();
    
    m_webView->handleInputEvent(event);
}

void BlinkWebRenderer::HandleKeyUp(int keyCode) {
    if (!m_webView)
        return;

    WebKeyboardEvent event;
    event.type = WebInputEvent::KeyUp;
    event.windowsKeyCode = sdlToWindowsVK(keyCode);
    event.nativeKeyCode = keyCode;
    event.modifiers = 0;
    event.text[0] = 0;
    event.unmodifiedText[0] = 0;
    
    m_webView->handleInputEvent(event);
}

void BlinkWebRenderer::HandleChar(int charCode) {
    if (!m_webView)
        return;

    WebKeyboardEvent event;
    event.type = WebInputEvent::Char;
    event.windowsKeyCode = charCode;
    event.modifiers = 0;
    event.text[0] = charCode;
    event.text[1] = 0;
    event.unmodifiedText[0] = charCode;
    event.unmodifiedText[1] = 0;
    
    m_webView->handleInputEvent(event);
    m_needsRender = true;
}

void BlinkWebRenderer::SetFocus(bool focused) {
    if (m_webView)
        m_webView->setFocus(focused);
}

void BlinkWebRenderer::Close() {
    if (!m_initialized)
        return;

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

}  // namespace html_viewer
