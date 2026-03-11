// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/simpleblink/blink_web_wrapper.h"
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
#include "public/platform/WebImage.h"
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
        return adoptWebPtr(new SimpleWebViewScheduler(m_mockTaskRunner.get()));
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

// 简单的 WebURLLoader 实现 - 用于 loadHTMLString
class SimpleWebURLLoader : public WebURLLoader {
public:
    SimpleWebURLLoader() : m_client(nullptr) {}
    ~SimpleWebURLLoader() override {}
    
    void loadSynchronously(const WebURLRequest& request,
                          WebURLResponse& response,
                          WebURLError& error,
                          WebData& data) override {
        // 同步加载 - 不支持
        error.reason = -1;
        error.domain = WebString::fromUTF8("SimpleWebURLLoader");
    }
    
    void loadAsynchronously(const WebURLRequest& request,
                           WebURLLoaderClient* client) override {
        m_client = client;
        m_request = request;
        
        // 对于 loadHTMLString，我们需要立即失败
        // 这样 Blink 会使用 fallback 机制
        WebURLError error;
        error.reason = -1;
        error.domain = WebString::fromUTF8("SimpleWebURLLoader");
        error.isCancellation = false;
        error.staleCopyInCache = false;
        
        if (m_client) {
            m_client->didFail(this, error);
        }
    }
    
    void cancel() override {
        m_client = nullptr;
    }
    
    void setDefersLoading(bool defers) override {}
    void setLoadingTaskRunner(WebTaskRunner*) override {}
    
private:
    WebURLLoaderClient* m_client;
    WebURLRequest m_request;
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
    
    // URL Loader - 返回简单的实现
    WebURLLoader* createURLLoader() override {
        return new SimpleWebURLLoader();
    }
    
    // 其他必需方法返回空实现
    WebBlobRegistry* blobRegistry() override { return nullptr; }
    WebFileSystem* fileSystem() override { return nullptr; }
    WebIDBFactory* idbFactory() override { return nullptr; }
    WebScrollbarBehavior* scrollbarBehavior() override { return nullptr; }
    WebClipboard* clipboard() override { return nullptr; }
    WebFileUtilities* fileUtilities() override { return nullptr; }
    WebMimeRegistry* mimeRegistry() override { return nullptr; }
    WebThemeEngine* themeEngine() override { return &theme_engine_; }
    WebCookieJar* cookieJar() override { return nullptr; }

private:
    OwnPtr<SimpleWebThread> m_thread;
    OwnPtr<cc_blink::WebCompositorSupportImpl> m_compositorSupport;
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
// 第三部分: WebViewClient 实现 (最小集)
// 参考 SimWebViewClient
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

    // 8. 分配像素缓冲区
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

    // 软件渲染: 使用 WebViewImpl::paint() 直接绘制到 SkCanvas
    // 注意: paint() 只能在合成未激活时使用 (setAcceleratedCompositingEnabled(false))
    
    // 创建 Skia 位图和画布
    SkImageInfo info = SkImageInfo::MakeN32Premul(m_width, m_height);
    SkBitmap bitmap;
    bitmap.allocPixels(info);
    
    SkCanvas canvas(bitmap);
    canvas.clear(SK_ColorWHITE);

    // 使用 WebView 的软件渲染 paint 方法
    // 这会调用 PageWidgetDelegate::paint() 进行软件绘制
    WebRect rect(0, 0, m_width, m_height);
    m_webView->paint(&canvas, rect);
    
    // 复制像素到输出缓冲区
    // 注意: Skia 使用 BGRA 格式，可能需要转换为 RGBA
    const uint8_t* srcPixels = static_cast<const uint8_t*>(bitmap.getPixels());
    
    // 检查是否有非白色像素
    bool hasContent = false;
    for (int i = 0; i < m_width * m_height; ++i) {
        // BGRA -> RGBA
        m_pixels[i * 4 + 0] = srcPixels[i * 4 + 2];  // R
        m_pixels[i * 4 + 1] = srcPixels[i * 4 + 1];  // G
        m_pixels[i * 4 + 2] = srcPixels[i * 4 + 0];  // B
        m_pixels[i * 4 + 3] = srcPixels[i * 4 + 3];  // A
        
        // 检查是否有非白色像素
        if (srcPixels[i * 4 + 0] != 0xFF || srcPixels[i * 4 + 1] != 0xFF || 
            srcPixels[i * 4 + 2] != 0xFF) {
            hasContent = true;
        }
    }
    
    if (!hasContent) {
        fprintf(stderr, "Warning: Rendered image is all white - no content rendered\n");
    } else {
        fprintf(stderr, "Rendered image has content\n");
    }
    
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
    event.clickCount = 0;
    
    m_webView->handleInputEvent(event);
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
            // 映射到 ButtonLeft 或忽略
            event.button = blink::WebMouseEvent::ButtonLeft;
            break;
        default:
            event.button = blink::WebMouseEvent::ButtonLeft;
            break;
    }
    
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

}  // namespace html_viewer
