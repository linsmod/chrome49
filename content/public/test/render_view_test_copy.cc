
#include "content/public/test/render_view_test.h"
#include "content/public/test/render_view_test_copy.h"
namespace content {
  class MyRenderViewTest:public RenderViewTest{
    void TestBody() override{
      // CommonNavigationParams common_params;
      // common_params.url = GURL("data:text/html,min_zoomlimit_test");
      // view()->OnSetZoomLevelForLoadingURL(common_params.url, kMinZoomLevel);
      // frame()->Navigate(common_params, StartNavigationParams(),
      //                   RequestNavigationParams());
      // ProcessPendingMessages();
    }
  };
  RenderViewTest* createRenderViewTest(){
    return new MyRenderViewTest();
  }
}