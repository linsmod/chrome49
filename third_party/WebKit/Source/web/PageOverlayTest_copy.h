// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/WebViewImpl.h"
#include "web/tests/FrameTestHelpers.h"
namespace blink {

static const int viewportWidth = 800;
static const int viewportHeight = 600;

template <bool(*getter)(), void(*setter)(bool)>
class RuntimeFeatureChange {
public:
    RuntimeFeatureChange(bool newValue) : m_oldValue(getter()) { setter(newValue); }
    ~RuntimeFeatureChange() { setter(m_oldValue); }
private:
    bool m_oldValue;
};

class PageOverlayTest_copy {
public:
    enum CompositingMode { AcceleratedCompositing, UnacceleratedCompositing };
    BLINK_EXPORT PageOverlayTest_copy();
    BLINK_EXPORT void initialize(CompositingMode compositingMode);

    BLINK_EXPORT WebViewImpl* webViewImpl() const;

    // template <typename OverlayType>
    BLINK_EXPORT void runPageOverlayTestWithAcceleratedCompositing();
    BLINK_EXPORT ~PageOverlayTest_copy();
private:
    FrameTestHelpers::WebViewHelper m_helper;
};

} // namespace blink
