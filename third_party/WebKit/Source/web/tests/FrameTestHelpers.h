/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef FrameTestHelpers_h
#define FrameTestHelpers_h

#include "core/frame/Settings.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "public/platform/WebURLRequest.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebHistoryItem.h"
#include "public/web/WebRemoteFrameClient.h"
#include "public/web/WebViewClient.h"
#include "web/WebViewImpl.h"
#include "wtf/PassOwnPtr.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>

// @lindsmod add include
#include "base/time/time.h"
#include "components/test_runner/web_task.h"
#include "public/platform/Platform.h"
#include "public/web/WebPagePopup.h"
namespace blink {

class WebFrameWidget;
class WebRemoteFrameImpl;

namespace FrameTestHelpers {

    class TestWebFrameClient;

    // Loads a url into the specified WebFrame for testing purposes. Pumps any
    // pending resource requests, as well as waiting for the threaded parser to
    // finish, before returning.
    void loadFrame(WebFrame*, const std::string& url);
    // Same as above, but for WebFrame::loadHTMLString().
    void loadHTMLString(WebFrame*, const std::string& html, const WebURL& baseURL);
    // Same as above, but for WebFrame::loadHistoryItem().
    void loadHistoryItem(WebFrame*, const WebHistoryItem&, WebHistoryLoadType, WebURLRequest::CachePolicy);
    // Same as above, but for WebFrame::reload().
    void reloadFrame(WebFrame*);
    void reloadFrameIgnoringCache(WebFrame*);

    // Pumps pending resource requests while waiting for a frame to load. Don't use
    // this. Use one of the above helpers.
    void pumpPendingRequestsDoNotUse(WebFrame*);

    class SettingOverrider {
    public:
        virtual void overrideSettings(WebSettings*) = 0;
    };

    // Forces to use mocked overlay scrollbars instead of the default native theme scrollbars to avoid
    // crash in Chromium code when it tries to load UI resources that are not available when running
    // blink unit tests, and to ensure consistent layout regardless of differences between scrollbar themes.
    // WebViewHelper includes this, so this is only needed if a test doesn't use WebViewHelper or the test
    // needs a bigger scope of mock scrollbar settings than the scope of WebViewHelper.
    class UseMockScrollbarSettings {
    public:
        UseMockScrollbarSettings()
            : m_originalMockScrollbarEnabled(Settings::mockScrollbarsEnabled())
            , m_originalOverlayScrollbarsEnabled(RuntimeEnabledFeatures::overlayScrollbarsEnabled())
        {
            Settings::setMockScrollbarsEnabled(true);
            RuntimeEnabledFeatures::setOverlayScrollbarsEnabled(true);
            EXPECT_TRUE(ScrollbarTheme::theme().usesOverlayScrollbars());
        }

        ~UseMockScrollbarSettings()
        {
            Settings::setMockScrollbarsEnabled(m_originalMockScrollbarEnabled);
            RuntimeEnabledFeatures::setOverlayScrollbarsEnabled(m_originalOverlayScrollbarsEnabled);
        }

    private:
        bool m_originalMockScrollbarEnabled;
        bool m_originalOverlayScrollbarsEnabled;
    };

    // Convenience class for handling the lifetime of a WebView and its associated mainframe in tests.
    class WebViewHelper {
        WTF_MAKE_NONCOPYABLE(WebViewHelper);

    public:
        WebViewHelper(SettingOverrider* = 0);
        ~WebViewHelper();

        // Creates and initializes the WebView. Implicitly calls reset() first. IF a
        // WebFrameClient or a WebViewClient are passed in, they must outlive the
        // WebViewHelper.
        WebViewImpl* initialize(bool enableJavascript = false, TestWebFrameClient* = 0, WebViewClient* = 0, void (*updateSettingsFunc)(WebSettings*) = 0);

        // Same as initialize() but also performs the initial load of the url. Only
        // returns once the load is complete.
        WebViewImpl* initializeAndLoad(const std::string& url, bool enableJavascript = false, TestWebFrameClient* = 0, WebViewClient* = 0, void (*updateSettingsFunc)(WebSettings*) = 0);

        void reset();

        WebView* webView() const { return m_webView; }
        WebViewImpl* webViewImpl() const { return m_webView; }

    private:
        WebViewImpl* m_webView;
        WebFrameWidget* m_webViewWidget;
        SettingOverrider* m_settingOverrider;
        UseMockScrollbarSettings m_mockScrollbarSettings;
    };

    // Minimal implementation of WebFrameClient needed for unit tests that load frames. Tests that load
    // frames and need further specialization of WebFrameClient behavior should subclass this.
    class TestWebFrameClient : public WebFrameClient {
    public:
        TestWebFrameClient();

        WebFrame* createChildFrame(WebLocalFrame* parent, WebTreeScopeType, const WebString& frameName, WebSandboxFlags, const WebFrameOwnerProperties&) override;
        void frameDetached(WebFrame*, DetachType) override;
        void didStartLoading(bool) override;
        void didStopLoading() override;

        bool isLoading() { return m_loadsInProgress > 0; }
        void waitForLoadToComplete();

    private:
        int m_loadsInProgress;
    };

    // Minimal implementation of WebRemoteFrameClient needed for unit tests that load remote frames. Tests that load
    // frames and need further specialization of WebFrameClient behavior should subclass this.
    class TestWebRemoteFrameClient : public WebRemoteFrameClient {
    public:
        TestWebRemoteFrameClient();

        WebRemoteFrameImpl* frame() const { return m_frame; }

        // WebRemoteFrameClient overrides:
        void frameDetached(DetachType) override;
        void postMessageEvent(
            WebLocalFrame* sourceFrame,
            WebRemoteFrame* targetFrame,
            WebSecurityOrigin targetOrigin,
            WebDOMMessageEvent) override { }

    private:
        RawPtrWillBePersistent<WebRemoteFrameImpl> const m_frame;
    };

    class TestWebViewClient : public WebViewClient {
    public:
        virtual ~TestWebViewClient() { }
        void Initialize(
            blink::WebWidget* widget)
        {
            widget_ = widget;
        }
        void initializeLayerTreeView() override;
        WebLayerTreeView* layerTreeView() override { return m_layerTreeView.get(); }
        void scheduleAnimation() override;
        WebWidget* widget()
        {
            return widget_;
        }
        void AnimateNow()
        {
            if (animate_scheduled_) {
                base::TimeDelta animate_time = base::TimeTicks::Now() - base::TimeTicks();
                animate_scheduled_ = false;

                widget()->beginFrame(animate_time.InSecondsF());
                widget()->updateAllLifecyclePhases();
                if (blink::WebPagePopup* popup = widget()->pagePopup()) {
                    popup->beginFrame(animate_time.InSecondsF());
                    popup->updateAllLifecyclePhases();
                }
            }
        }
        class InvokeTaskHelper : public blink::WebTaskRunner::Task {
        public:
            InvokeTaskHelper(scoped_ptr<test_runner::WebTask> task)
                : task_(std::move(task))
            {
            }

            // WebThread::Task implementation:
            void run() override { task_->run(); }

        private:
            scoped_ptr<test_runner::WebTask> task_;
        };
        void PostDelayedTask(test_runner::WebTask* task,
            long long ms)
        {
            Platform::current()->currentThread()->taskRunner()->postDelayedTask(
                WebTraceLocation(__FUNCTION__, __FILE__),
                new InvokeTaskHelper(make_scoped_ptr(task)), ms);
        }
        test_runner::WebTaskList* mutable_task_list() { return &task_list_; }

    private:
        OwnPtr<WebLayerTreeView> m_layerTreeView;
        bool animate_scheduled_ = false;
        blink::WebWidget* widget_ = nullptr;
        test_runner::WebTaskList task_list_;
    };

} // namespace FrameTestHelpers
} // namespace blink

#endif // FrameTestHelpers_h
