// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/PageOverlay.h"

#include "core/frame/FrameView.h"
#include "core/layout/LayoutView.h"
#include "url/gurl.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLRequest.h"
#include "platform/graphics/Color.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/PaintController.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCanvas.h"
#include "public/platform/WebThread.h"
#include "public/web/WebSettings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"
#include "web/tests/FrameTestHelpers.h"
#include "web/WebViewImplUpdateListener.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "core/layout/compositing/CompositedLayerMapping.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "platform/testing/URLTestHelpers.h"
#include "base/run_loop.h"



using testing::_;
using testing::AtLeast;
using testing::Property;



namespace blink {
    static blink::WebViewImplUpdateListener* m_wk_update_listener;
    void webkitSetWebViewImplUpdateListener(blink::WebViewImplUpdateListener* listener)
    {
        m_wk_update_listener = listener;
    }
namespace {


static const int viewportWidth = 800;
static const int viewportHeight = 600;

// These unit tests cover both PageOverlay and PageOverlayList.

void enableAcceleratedCompositing(WebSettings* settings)
{
    settings->setAcceleratedCompositingEnabled(true);
}

void disableAcceleratedCompositing(WebSettings* settings)
{
    settings->setAcceleratedCompositingEnabled(false);
}

static CompositedLayerMapping* mappingFromElement(Element* element)
{
    if (!element)
        return nullptr;
    LayoutObject* layoutObject = element->layoutObject();
    if (!layoutObject || !layoutObject->isBoxModelObject())
        return nullptr;
    PaintLayer* layer = toLayoutBoxModelObject(layoutObject)->layer();
    if (!layer)
        return nullptr;
    if (!layer->hasCompositedLayerMapping())
        return nullptr;
    return layer->compositedLayerMapping();
}

static WebLayer* webLayerFromGraphicsLayer(GraphicsLayer* graphicsLayer)
{
    if (!graphicsLayer)
        return nullptr;
    return graphicsLayer->platformLayer();
}

// static WebLayer* scrollingWebLayerFromElement(Element* element)
// {
//     CompositedLayerMapping* compositedLayerMapping = mappingFromElement(element);
//     if (!compositedLayerMapping)
//         return nullptr;
//     return webLayerFromGraphicsLayer(compositedLayerMapping->scrollingContentsLayer());
// }

static WebLayer* webLayerFromElement(Element* element)
{
    CompositedLayerMapping* compositedLayerMapping = mappingFromElement(element);
    if (!compositedLayerMapping)
        return nullptr;
    return webLayerFromGraphicsLayer(compositedLayerMapping->mainGraphicsLayer());
}
class PageOverlayTest : public ::testing::Test {
protected:
     PageOverlayTest()
        : m_baseURL("http://www.test.com/")
    {

    }
    enum CompositingMode { AcceleratedCompositing, UnacceleratedCompositing };
    WebViewImpl* webViewImpl() const { return m_helper.webViewImpl(); }
    LocalFrame* frame() const { return m_helper.webViewImpl()->mainFrameImpl()->frame(); }
    void navigateTo(const String& url)
    {
        FrameTestHelpers::loadFrame(webViewImpl()->mainFrame(), url.utf8().data());
    }

    void forceFullCompositingUpdate()
    {
        webViewImpl()->updateAllLifecyclePhases();
    }

    WebLayer* getRootScrollLayer()
    {
        PaintLayerCompositor* compositor = frame()->contentLayoutObject()->compositor();
        ASSERT(compositor);
        ASSERT(compositor->scrollLayer());

        WebLayer* webScrollLayer = compositor->scrollLayer()->platformLayer();
        return webScrollLayer;
    }
    void initialize(CompositingMode compositingMode)
    {
        RuntimeEnabledFeatures::setCompositorWorkerEnabled(true);
        m_helper.initialize(
            true /* enableJavascript */, nullptr /* webFrameClient */, nullptr /* webViewClient */,
            compositingMode == AcceleratedCompositing ? enableAcceleratedCompositing : disableAcceleratedCompositing);
        webViewImpl()->resize(WebSize(viewportWidth, viewportHeight));
        webViewImpl()->updateAllLifecyclePhases();
        ASSERT_EQ(compositingMode == AcceleratedCompositing, webViewImpl()->isAcceleratedCompositingActive());
if(1){
    return;
}
        registerMockedHttpURLLoad("compositor-proxy-basic.html");
        navigateTo(m_baseURL + "compositor-proxy-basic.html");

        forceFullCompositingUpdate();

        Document* document = frame()->document();

        Element* tallElement = document->getElementById("tall");
        WebLayer* tallLayer = webLayerFromElement(tallElement);
        EXPECT_TRUE(!tallLayer);
    }

    void registerMockedHttpURLLoad(const std::string& fileName)
    {
        URLTestHelpers::registerMockedURLFromBaseURL(m_baseURL, WebString::fromUTF8(fileName.c_str()));
    }
    template <typename OverlayType>
    void runPageOverlayTestWithAcceleratedCompositing();

private:
    String m_baseURL;
    FrameTestHelpers::WebViewHelper m_helper;
};

// PageOverlay that paints a solid color.
class SolidColorOverlay : public PageOverlay::Delegate {
public:
    SolidColorOverlay(Color color) : m_color(color) { }

    void paintPageOverlay(const PageOverlay& pageOverlay, GraphicsContext& graphicsContext, const WebSize& size) const override
    {
        if (DrawingRecorder::useCachedDrawingIfPossible(graphicsContext, pageOverlay, DisplayItem::PageOverlay))
            return;
        FloatRect rect(0, 0, size.width, size.height);
        DrawingRecorder drawingRecorder(graphicsContext, pageOverlay, DisplayItem::PageOverlay, rect);
        graphicsContext.fillRect(rect, m_color);
    }

private:
    Color m_color;
};

template <bool(*getter)(), void(*setter)(bool)>
class RuntimeFeatureChange {
public:
    RuntimeFeatureChange(bool newValue) : m_oldValue(getter()) { setter(newValue); }
    ~RuntimeFeatureChange() { setter(m_oldValue); }
private:
    bool m_oldValue;
};
WebURLRequest createDataRequest(const std::string& html) {
  std::string url_str = "data:text/html;charset=utf-8,";
  url_str.append(html);
  GURL url(url_str);
  WebURLRequest request(url);
  request.setCheckForBrowserSideNavigation(false);
  return request;
}
class MockCanvas : public SkCanvas {
public:
    MockCanvas(int width, int height) : SkCanvas(width, height) { }
    MOCK_METHOD2(onDrawRect, void(const SkRect&, const SkPaint&));
};

TEST_F(PageOverlayTest, PageOverlay_AcceleratedCompositing)
{
    initialize(AcceleratedCompositing);
    webViewImpl()->layerTreeView()->setViewportSize(WebSize(viewportWidth, viewportHeight));

    OwnPtr<PageOverlay> pageOverlay = PageOverlay::create(webViewImpl(), new SolidColorOverlay(SK_ColorYELLOW));
    pageOverlay->update();
    // m_wk_update_listener->onUpdate(webViewImpl());
    std::string html = "<body style=\"background-color:green;\"></body>";
    webViewImpl()->mainFrameImpl()->loadRequest(createDataRequest(html));
    webViewImpl()->updateAllLifecyclePhases();

    // Ideally, we would get results from the compositor that showed that this
    // page overlay actually winds up getting drawn on top of the rest.
    // For now, we just check that the GraphicsLayer will draw the right thing.

    // MockCanvas canvas(viewportWidth, viewportHeight);
    // EXPECT_CALL(canvas, onDrawRect(_, _)).Times(AtLeast(0));
    // EXPECT_CALL(canvas, onDrawRect(SkRect::MakeWH(viewportWidth, viewportHeight), Property(&SkPaint::getColor, SK_ColorYELLOW)));

    // GraphicsLayer* graphicsLayer = pageOverlay->graphicsLayer();
    // WebRect rect(0, 0, viewportWidth, viewportHeight);

    // // Paint the layer with a null canvas to get a display list, and then
    // // replay that onto the mock canvas for examination.
    // IntRect intRect = rect;
    // graphicsLayer->paint(&intRect);

    // PaintController& paintController = graphicsLayer->paintController();
    // GraphicsContext graphicsContext(paintController);
    // graphicsContext.beginRecording(intRect);
    // paintController.paintArtifact().replay(graphicsContext);
    // graphicsContext.endRecording()->playback(&canvas);
    base::RunLoop().Run();
}

} // namespace
} // namespace blink
