// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/PageOverlay.h"
#include <content/test/blink_test_environment.h>
#include "core/frame/FrameView.h"
#include "core/layout/LayoutView.h"
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
#include "web/PageOverlayTest_copy.h"
namespace blink {

// These unit tests cover both PageOverlay and PageOverlayList.

void enableAcceleratedCompositing(WebSettings* settings)
{
    settings->setAcceleratedCompositingEnabled(true);
}

void disableAcceleratedCompositing(WebSettings* settings)
{
    settings->setAcceleratedCompositingEnabled(false);
}


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

class MockCanvas : public SkCanvas {
public:
    MockCanvas(int width, int height) : SkCanvas(width, height) { }
};

    PageOverlayTest_copy::PageOverlayTest_copy(){
        // content::SetUpBlinkTestEnvironment();
        this->initialize(PageOverlayTest_copy::CompositingMode::AcceleratedCompositing);
    }
    void PageOverlayTest_copy::initialize(CompositingMode compositingMode)
    {
        m_helper.initialize(
            false /* enableJavascript */, nullptr /* webFrameClient */, nullptr /* webViewClient */,
            compositingMode == AcceleratedCompositing ? enableAcceleratedCompositing : disableAcceleratedCompositing);
        webViewImpl()->resize(WebSize(viewportWidth, viewportHeight));
        webViewImpl()->updateAllLifecyclePhases();
        ASSERT_EQ(compositingMode == AcceleratedCompositing, webViewImpl()->isAcceleratedCompositingActive());
    }

    WebViewImpl* PageOverlayTest_copy::webViewImpl() const { return m_helper.webViewImpl(); }

    // template <typename OverlayType>
    void PageOverlayTest_copy::runPageOverlayTestWithAcceleratedCompositing()
    {
        // initialize(AcceleratedCompositing);
        webViewImpl()->layerTreeView()->setViewportSize(WebSize(viewportWidth, viewportHeight));

        OwnPtr<PageOverlay> pageOverlay = PageOverlay::create(webViewImpl(), new SolidColorOverlay(SK_ColorYELLOW));
        pageOverlay->update();
        webViewImpl()->updateAllLifecyclePhases();

        // Ideally, we would get results from the compositor that showed that this
        // page overlay actually winds up getting drawn on top of the rest.
        // For now, we just check that the GraphicsLayer will draw the right thing.

        MockCanvas canvas(viewportWidth, viewportHeight);

        GraphicsLayer* graphicsLayer = pageOverlay->graphicsLayer();
        WebRect rect(0, 0, viewportWidth, viewportHeight);

        // Paint the layer with a null canvas to get a display list, and then
        // replay that onto the mock canvas for examination.
        IntRect intRect = rect;
        graphicsLayer->paint(&intRect);

        PaintController& paintController = graphicsLayer->paintController();
        GraphicsContext graphicsContext(paintController);
        graphicsContext.beginRecording(intRect);
        paintController.paintArtifact().replay(graphicsContext);
        graphicsContext.endRecording()->playback(&canvas);
    }
    PageOverlayTest_copy::~PageOverlayTest_copy(){
        // content::TearDownBlinkTestEnvironment();
    }
} // namespace blink
