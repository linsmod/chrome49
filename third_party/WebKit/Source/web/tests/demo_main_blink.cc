// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/message_loop/message_loop.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_device_source.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/env.h"
#include "ui/aura/test/test_focus_client.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/compositor/test/in_process_context_factory.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/skia_util.h"
#include "ui/gl/gl_surface.h"
#include <memory>

#if defined(USE_X11)
#include "ui/gfx/x/x11_connection.h"
#endif

#if defined(OS_WIN)
#include "ui/gfx/win/dpi.h"
#endif

#include "ui/base/ime/input_method_initializer.h"

// #include "web/PageOverlayTest_copy.h"
#include "wtf/Partitions.h"
#include "gin/v8_initializer.h"

#include "public/platform/Platform.h"
#include "public/platform/WebUnitTestSupport.h"
#include "public/web/WebKit.h"
#include "web/tests/WebUnitTests.h"


#include "web/WebLocalFrameImpl.h"
#include "web/WebSettingsImpl.h"
#include "web/WebViewImpl.h"
#include "web/tests/FrameTestHelpers.h"

#include "platform/testing/TestingPlatformSupport.h"

#include "third_party/WebKit/public/platform/WebData.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/Source/web/tests/FrameTestHelpers.h"

#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/Source/web/WebViewImpl.h"

#include "components/scheduler/child/scheduler_tqm_delegate_impl.h"
#include "components/scheduler/child/web_task_runner_impl.h"
#include "components/scheduler/renderer/renderer_scheduler_impl.h"
#include "components/scheduler/renderer/renderer_web_scheduler_impl.h"
#include "components/scheduler/test/lazy_scheduler_message_loop_delegate_for_tests.h"

#include "content/test/web_layer_tree_view_impl_for_testing.h"

namespace {
// Trivial WindowDelegate implementation that draws a colored background.
class DemoWindowDelegate : public aura::WindowDelegate {
 public:
  explicit DemoWindowDelegate(SkColor color, base::Closure quit_closure, blink::WebViewImpl* web_view)
      : color_(color), quit_closure_(quit_closure), web_view_(web_view) {}

  // Overridden from WindowDelegate:
  gfx::Size GetMinimumSize() const override { return gfx::Size(); }

  gfx::Size GetMaximumSize() const override { return gfx::Size(); }

  void OnBoundsChanged(const gfx::Rect& old_bounds,
                       const gfx::Rect& new_bounds) override {
    window_bounds_ = new_bounds;
  }
  gfx::NativeCursor GetCursor(const gfx::Point& point) override {
    return gfx::kNullCursor;
  }
  int GetNonClientComponent(const gfx::Point& point) const override {
    return HTCAPTION;
  }
 void OnMouseEvent(ui::MouseEvent* event) override {
    if (event->type() == ui::ET_MOUSE_PRESSED) {
      DCHECK(!quit_closure_.is_null());
      quit_closure_.Run();
    }
  }
  bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) override {
    return true;
  }
  bool CanFocus() override { return true; }
  void OnCaptureLost() override {}
  void OnPaint(const ui::PaintContext& context) override {
    ui::PaintRecorder recorder(context, window_bounds_.size());
    gfx::Canvas* canvas = recorder.canvas();

    // 绘制背景
    canvas->DrawColor(color_, SkXfermode::kSrc_Mode);

    // 渲染 WebView
    if (web_view_) {
        blink::WebRect webRect(window_bounds_.x(), window_bounds_.y(), 
                               window_bounds_.width(), window_bounds_.height());
        web_view_->paint(canvas->sk_canvas(), webRect);
    }
  }
  void OnDeviceScaleFactorChanged(float device_scale_factor) override {}
  void OnWindowDestroying(aura::Window* window) override {
  }
  void OnWindowDestroyed(aura::Window* window) override {

  }
  void OnWindowTargetVisibilityChanged(bool visible) override {}
  bool HasHitTestMask() const override { return false; }
  void GetHitTestMask(gfx::Path* mask) const override {}

 private:
  SkColor color_;
  gfx::Rect window_bounds_;
  base::Closure quit_closure_;
  blink::WebViewImpl* web_view_;  // 新增：存储 WebView 指针
  DISALLOW_COPY_AND_ASSIGN(DemoWindowDelegate);
};

class DemoWindowTreeClient : public aura::client::WindowTreeClient {
 public:
  explicit DemoWindowTreeClient(aura::Window* window) : window_(window) {}
  ~DemoWindowTreeClient() override {}

  // Overridden from aura::client::WindowTreeClient:
  aura::Window* GetDefaultParent(aura::Window* context,
                                 aura::Window* window,
                                 const gfx::Rect& bounds) override {
    return window_;
  }

 private:
  aura::Window* window_;
  DISALLOW_COPY_AND_ASSIGN(DemoWindowTreeClient);
};
scoped_ptr<DemoWindowTreeClient> window_tree_client;
scoped_ptr<aura::WindowTreeHost> host;
scoped_ptr<aura::TestScreen> test_screen;
scoped_ptr<aura::Window> window1;
scoped_ptr<ui::InProcessContextFactory> context_factory;
scoped_ptr<DemoWindowDelegate> window_delegate1;


class CurrentThreadMock : public blink::WebThread {
 public:
  CurrentThreadMock()
      : task_runner_delegate_(
            scheduler::LazySchedulerMessageLoopDelegateForTests::Create()),
        scheduler_(
            new scheduler::RendererSchedulerImpl(task_runner_delegate_.get())),
        web_scheduler_(
            new scheduler::RendererWebSchedulerImpl(scheduler_.get())),
        web_task_runner_(
            new scheduler::WebTaskRunnerImpl(scheduler_->DefaultTaskRunner())) {
  }

  ~CurrentThreadMock() override {
    scheduler_->Shutdown();
  }

  blink::WebTaskRunner* taskRunner() override { return web_task_runner_.get(); }

  bool isCurrentThread() const override { return true; }

  blink::PlatformThreadId threadId() const override { return 17; }

  blink::WebScheduler* scheduler() const override {
    return web_scheduler_.get();
  }

  scheduler::RendererSchedulerImpl* scheduler_impl() const {
    return scheduler_.get();
  }

 private:
  scoped_refptr<scheduler::SchedulerTqmDelegate> task_runner_delegate_;
  scoped_ptr<scheduler::RendererSchedulerImpl> scheduler_;
  scoped_ptr<blink::WebScheduler> web_scheduler_;
  scoped_ptr<blink::WebTaskRunner> web_task_runner_;
};
class MockBlinkPlatform : NON_EXPORTED_BASE(public blink::Platform),public blink::WebUnitTestSupport {
 public:
  MockBlinkPlatform() {
    blink::initialize(this);
  }
  ~MockBlinkPlatform() override {}

  blink::WebThread* currentThread() override{
      return &m_currentThread ;
  }
  CurrentThreadMock* currentThread2(){
      return &m_currentThread ;
  }
  blink::WebUnitTestSupport* unitTestSupport() {
    return this;
  }

  blink::WebLayerTreeView* createLayerTreeViewForTesting() {
  scoped_ptr<content::WebLayerTreeViewImplForTesting> view(
      new content::WebLayerTreeViewImplForTesting());

  view->Initialize();
  return view.release();
}

 private:
  CurrentThreadMock m_currentThread;
  DISALLOW_COPY_AND_ASSIGN(MockBlinkPlatform);
};

// base::LazyInstance<MockBlinkPlatform>::Leaky g_mock_blink_platform =
//     LAZY_INSTANCE_INITIALIZER;

}  // namespace
int DemoMain();
int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);

  // The exit manager is in charge of calling the dtors of singleton objects.
  base::AtExitManager exit_manager;

  // base::i18n::InitializeICU();
  WTF::Partitions::initialize(nullptr);

  #if defined(V8_USE_EXTERNAL_STARTUP_DATA)
    gin::V8Initializer::LoadV8Snapshot();
    gin::V8Initializer::LoadV8Natives();
  #endif
  
  

  DemoMain();

  blink::shutdown();
  return 0;
}
int DemoMain() {
  
  std::string url = "data:text/html,<html><body>Hello, World!</body></html>";
  
  ui::InitializeInputMethodForTesting();
#if defined(USE_X11)
  // This demo uses InProcessContextFactory which uses X on a separate Gpu
  // thread.
  gfx::InitializeThreadedX11();
#endif

  gfx::GLSurface::InitializeOneOff();

#if defined(OS_WIN)
  gfx::SetDefaultDeviceScaleFactor(1.0f);
#endif

// The ContextFactory must exist before any Compositors are created.
  bool context_factory_for_test = false;
  context_factory.reset(
      new ui::InProcessContextFactory(context_factory_for_test, nullptr));
  context_factory->set_use_test_surface(false);

  // Create the message-loop here before creating the root window.
  base::MessageLoopForUI message_loop;
  // base::MessageLoopForUI::current()->SetTaskRunner(platform.currentThread2()->scheduler_impl()->DefaultTaskRunner());
  base::RunLoop run_loop;
  base::Closure quit_ui_task_runner = base::Bind(
      base::IgnoreResult(&base::SingleThreadTaskRunner::PostTask),
      message_loop.task_runner(),
      FROM_HERE, run_loop.QuitClosure());
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner = new base::SingleThreadTaskRunner(
      message_loop.task_runner(), quit_ui_task_runner);

  MockBlinkPlatform platform;  // 这将初始化 Blink

  base::PowerMonitor power_monitor(make_scoped_ptr(
      new base::PowerMonitorDeviceSource));

  aura::Env::CreateInstance(true);
  aura::Env::GetInstance()->set_context_factory(context_factory.get());
  test_screen.reset(
      aura::TestScreen::Create(gfx::Size()));
  gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, test_screen.get());
  host.reset(
      test_screen->CreateHostForPrimaryDisplay());
  window_tree_client.reset(
      new DemoWindowTreeClient(host->window()));
  aura::test::TestFocusClient focus_client;
  aura::client::SetFocusClient(host->window(), &focus_client);

  // Create a hierarchy of test windows.
  gfx::Rect window1_bounds(100, 100, 400, 400);
  blink::WebView* webView = nullptr;
  {
    blink::FrameTestHelpers::WebViewHelper webViewHelper;
    webView = webViewHelper.initialize();
    webView->resize(blink::WebSize(400, 400));
    // 使用 WebURL 和 WebData
    blink::WebURL webUrl(blink::KURL(blink::ParsedURLString, url.c_str()));
    blink::WebData webData(url.data(), url.size());
    webView->mainFrame()->loadHTMLString(webData, webUrl);
    webView->updateAllLifecyclePhases();
  }
  window_delegate1.reset(new DemoWindowDelegate(SK_ColorBLUE, run_loop.QuitClosure(), static_cast<blink::WebViewImpl*>(webView)));
  window1.reset(new aura::Window (window_delegate1.get()));
  window1->set_id(1);
  window1->Init(ui::LAYER_TEXTURED);
  window1->SetBounds(window1_bounds);
  window1->Show();
  aura::client::ParentWindowWithContext(window1.get(), host->window(), gfx::Rect());

  host->Show();

  // base::MessageLoopForUI::current()->Run();
   run_loop.Run();
  ui_task_runner = nullptr;
  return 0;
}