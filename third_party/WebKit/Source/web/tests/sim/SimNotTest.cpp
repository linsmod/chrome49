// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/tests/sim/SimNotTest.h"

#include "core/dom/Document.h"
#include "platform/LayoutTestSupport.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"

namespace blink {

SimNotTest::SimNotTest()
    : m_webViewClient(m_compositor)
{
    Document::setThreadedParsingEnabledForTesting(false);
    // Use the mock theme to get more predictable code paths, this also avoids
    // the OS callbacks in ScrollAnimatorMac which can schedule frames
    // unpredictably since the OS will randomly call into blink for
    // updateScrollerStyleForNewRecommendedScrollerStyle which then does
    // FrameView::scrollbarStyleChanged and will adjust the scrollbar existence
    // in the middle of a test.
    LayoutTestSupport::setMockThemeEnabledForTest(true);
    ScrollbarTheme::setMockScrollbarsEnabled(true);
    m_webViewHelper.initialize(true, nullptr, &m_webViewClient);
    m_compositor.setWebViewImpl(webView());
}

SimNotTest::~SimNotTest()
{
    Document::setThreadedParsingEnabledForTesting(true);
    LayoutTestSupport::setMockThemeEnabledForTest(false);
    ScrollbarTheme::setMockScrollbarsEnabled(false);
}

void SimNotTest::loadURL(const String& url)
{
    WebURLRequest request;
    request.initialize();
    request.setURL(KURL(ParsedURLString, url));
    webView().mainFrameImpl()->loadRequest(request);
}

Document& SimNotTest::document()
{
    return *webView().mainFrameImpl()->frame()->document();
}

WebViewImpl& SimNotTest::webView()
{
    return *m_webViewHelper.webViewImpl();
}

const SimWebViewClient& SimNotTest::webViewClient() const
{
    return m_webViewClient;
}

SimCompositor& SimNotTest::compositor()
{
    return m_compositor;
}

} // namespace blink
