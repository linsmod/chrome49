
#include "content/public/test/render_view_test.h"
#include "content/public/test/render_view_test_copy.h"
#include "content/common/navigation_params.h"
#include "content/public/common/page_zoom.h"
#include "content/test/test_render_frame.h"
#include "content/renderer/render_view_impl.h"
#include "base/message_loop/message_loop.h"

namespace content {
  const double kMinZoomLevel = ZoomFactorToZoomLevel(kMinimumZoomFactor);
    const double kMaxZoomLevel = ZoomFactorToZoomLevel(kMaximumZoomFactor);
  class MyRenderViewTest:public RenderViewTest{
    public:
    MyRenderViewTest(base::MessageLoop* message_loop_for_app_ui):RenderViewTest(message_loop_for_app_ui){
      
    }
    void TestBody() override{}
    void RunMyTestBody() override{
      CommonNavigationParams common_params;
      common_params.url = GURL("data:text/html,min_zoomlimit_test");
      view()->OnSetZoomLevelForLoadingURL(common_params.url, kMinZoomLevel);
      frame()->Navigate(common_params, StartNavigationParams(),
                        RequestNavigationParams());
      // ProcessPendingMessages();
      GetWebView()->updateAllLifecyclePhases();
    }
  
  RenderViewImpl* view() {
    return static_cast<RenderViewImpl*>(view_);
  }

  TestRenderFrame* frame() {
    return static_cast<TestRenderFrame*>(view()->GetMainRenderFrame());
  }
  };

  RenderViewTest* createRenderViewTest(base::MessageLoop* message_loop_for_app_ui){
    return new MyRenderViewTest(message_loop_for_app_ui);
  }
}