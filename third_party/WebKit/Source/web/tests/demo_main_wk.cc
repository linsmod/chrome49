// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
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
#include "content/public/test/render_view_test_copy.h"
#include "gin/v8_initializer.h"

#include "public/platform/Platform.h"
#include "public/web/WebKit.h"
#include "web/tests/WebUnitTests.h"
#include <content/test/blink_test_environment.h>

namespace {
// Trivial WindowDelegate implementation that draws a colored background.
class DemoWindowDelegate : public aura::WindowDelegate {
 public:
  explicit DemoWindowDelegate(SkColor color) : color_(color) {}

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
  void OnMouseEvent(ui::MouseEvent* event) override{
     
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
    recorder.canvas()->DrawColor(color_, SkXfermode::kSrc_Mode);
    gfx::Rect r;
    recorder.canvas()->GetClipBounds(&r);
    // Fill with a non-solid color so that the compositor will exercise its
    // texture upload path.
    while (!r.IsEmpty()) {
      r.Inset(2, 2);
      recorder.canvas()->FillRect(r, color_, SkXfermode::kXor_Mode);
    }

    
  }
  void OnDeviceScaleFactorChanged(float device_scale_factor) override {}
  void OnWindowDestroying(aura::Window* window) override {}
  void OnWindowDestroyed(aura::Window* window) override {}
  void OnWindowTargetVisibilityChanged(bool visible) override {}
  bool HasHitTestMask() const override { return false; }
  void GetHitTestMask(gfx::Path* mask) const override {}

 private:
  SkColor color_;
  gfx::Rect window_bounds_;

  DISALLOW_COPY_AND_ASSIGN(DemoWindowDelegate);
};

class DemoWindowTreeClient : public aura::client::WindowTreeClient {
 public:
  explicit DemoWindowTreeClient(aura::Window* window) : window_(window) {
    aura::client::SetWindowTreeClient(window_, this);
  }

  ~DemoWindowTreeClient() override {
    aura::client::SetWindowTreeClient(window_, nullptr);
  }

  // Overridden from aura::client::WindowTreeClient:
  aura::Window* GetDefaultParent(aura::Window* context,
                                 aura::Window* window,
                                 const gfx::Rect& bounds) override {
    if (!capture_client_) {
      capture_client_.reset(
          new aura::client::DefaultCaptureClient(window_->GetRootWindow()));
    }
    return window_;
  }

 private:
  aura::Window* window_;

  scoped_ptr<aura::client::DefaultCaptureClient> capture_client_;

  DISALLOW_COPY_AND_ASSIGN(DemoWindowTreeClient);
};
scoped_ptr<DemoWindowTreeClient> window_tree_client;
scoped_ptr<aura::WindowTreeHost> host;
scoped_ptr<aura::TestScreen> test_screen;
scoped_ptr<aura::Window> window1;
scoped_ptr<ui::InProcessContextFactory> context_factory;
scoped_ptr<DemoWindowDelegate> window_delegate1;
int DemoMain() {
  
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
  // base::MessageLoopForUI message_loop;
 

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
  window_delegate1.reset(new DemoWindowDelegate(SK_ColorBLUE));
  window1.reset(new aura::Window (window_delegate1.get()));
  window1->set_id(1);
  window1->Init(ui::LAYER_TEXTURED);
  window1->SetBounds(window1_bounds);
  window1->Show();
  aura::client::ParentWindowWithContext(window1.get(), host->window(), gfx::Rect());

  host->Show();
  // base::MessageLoopForUI::current()->Run();
  return 0;
}

void preTestHook()
{
  content::SetUpBlinkTestEnvironment();
  DemoMain();
}

void postTestHook()
{
    base::MessageLoopForUI::current()->Run();
    // content::TearDownBlinkTestEnvironment();
}

}  // namespace
int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);

  // The exit manager is in charge of calling the dtors of singleton objects.
  // base::AtExitManager exit_manager;

  // base::i18n::InitializeICU();
  WTF::Partitions::initialize(nullptr);

  #if defined(V8_USE_EXTERNAL_STARTUP_DATA)
    gin::V8Initializer::LoadV8Snapshot();
    gin::V8Initializer::LoadV8Natives();
  #endif
  
  
  
  return blink::runWebTests(argc, argv, &preTestHook, &postTestHook);
}
