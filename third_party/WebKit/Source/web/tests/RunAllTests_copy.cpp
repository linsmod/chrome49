/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "public/web/WebKit.h"

#include "public/web/WebView.h"

#include "bindings/core/v8/V8Document.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/markers/DocumentMarkerController.h"
#include "core/frame/EventHandlerRegistry.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/VisualViewport.h"
#include "core/html/HTMLDocument.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLTextAreaElement.h"
#include "core/layout/LayoutView.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/page/Page.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/PaintLayerPainter.h"
#include "core/timing/DOMWindowPerformance.h"
#include "core/timing/Performance.h"
#include "core/timing/PerformanceCompositeTiming.h"
#include "platform/KeyboardCodes.h"
#include "platform/UserGestureIndicator.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/Color.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/SkPictureBuilder.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebClipboard.h"
#include "public/platform/WebDisplayMode.h"
#include "public/platform/WebDragData.h"
#include "public/platform/WebSize.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebUnitTestSupport.h"
#include "public/web/WebAutofillClient.h"
#include "public/web/WebContentDetectionResult.h"
#include "public/web/WebDateTimeChooserCompletion.h"
#include "public/web/WebDeviceEmulationParams.h"
#include "public/web/WebDocument.h"
#include "public/web/WebDragOperation.h"
#include "public/web/WebElement.h"
#include "public/web/WebFrame.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebHitTestResult.h"
#include "public/web/WebInputEvent.h"
#include "public/web/WebScriptSource.h"
#include "public/web/WebSettings.h"
#include "public/web/WebTreeScopeType.h"
#include "public/web/WebViewClient.h"
#include "public/web/WebWidget.h"
#include "public/web/WebWidgetClient.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebSettingsImpl.h"
#include "web/WebViewImpl.h"
#include "web/tests/FrameTestHelpers.h"

#include "platform/testing/TestingPlatformSupport.h"
#include "url/url_util.h"
#include "wtf/Partitions.h"
#include "components/scheduler/renderer/renderer_scheduler.h"
#include "components/html_viewer/blink_platform_impl.h"

 
#include "third_party/WebKit/public/platform/WebURLLoader.h"
#include "content/renderer/renderer_blink_platform_impl.h"
class RendererBlinkPlatformImplTestOverrideImpl_copy
    : public RendererBlinkPlatformImpl {
 public:
  RendererBlinkPlatformImplTestOverrideImpl_copy(
      scheduler::RendererScheduler* scheduler)
      : RendererBlinkPlatformImpl(scheduler) {}

  // Get rid of the dependency to the sandbox, which is not available in
  // RenderViewTest.
  blink::WebSandboxSupport* sandboxSupport() override { return NULL; }

  // Inject a WebURLLoader which rewrites requests that have the
  // X-WrappedHTMLData header.
  WebURLLoader* createURLLoader() override {
    return new WebURLLoaderWrapper(
        RendererBlinkPlatformImpl::createURLLoader());
  }
};

int main(int argc, char** argv)
{
    scoped_ptr<scheduler::RendererScheduler> renderer_scheduler_;
    scoped_ptr<RendererBlinkPlatformImplTestOverrideImpl_copy> blink_platform_;
    blink::TestingPlatformSupport m_testingPlatformSupport;

    // int int LaunchUnitTests
    base::CommandLine::Init(argc, argv);
    // base::MessageLoopForUI loop;
    // // Tickle EndOfTaskRunner which among other things will flush the queue
    // // of error messages via V8Initializer::reportRejectedPromisesOnMainThread.
    // base::MessageLoop::current()->PostTask(FROM_HERE, base::Bind(&base::DoNothing));
    // base::RunLoop().RunUntilIdle();
    url::Initialize();
    WTF::Partitions::initialize(nullptr);

    renderer_scheduler_.reset(scheduler::RendererScheduler::Create());
    blink_platform_.reset(new RendererBlinkPlatformImplTestOverrideImpl_copy(renderer_scheduler_.get()));

    blink::initialize(blink_platform_.get());
    std::string url = "https://baidu.com";
    blink::FrameTestHelpers::WebViewHelper m_webViewHelper;
    blink::WebViewImpl* webView = m_webViewHelper.initializeAndLoad(url, true);
    webView->updateAllLifecyclePhases();
    m_webViewHelper.reset();
}

