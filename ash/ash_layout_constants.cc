// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_layout_constants.h"

#include "base/logging.h"
#include "ui/base/resource/material_design/material_design_controller.h"

gfx::Size GetAshLayoutSize(AshLayoutSize size) {
  const int kBrowserMaximizedCaptionButtonHeight[] = {27, 29, 33};
  const int kBrowserMaximizedCaptionButtonWidth[] = {35, 32, 32};
  const int kBrowserRestoredCaptionButtonHeight[] = {35, 36, 40};
  const int kBrowserRestoredCaptionButtonWidth[] = {35, 32, 32};
  const int kNonBrowserCaptionButtonHeight[] = {33, 33, 33};
  const int kNonBrowserCaptionButtonWidth[] = {32, 32, 32};

  const int mode = ui::MaterialDesignController::GetMode();
  switch (size) {
    case AshLayoutSize::BROWSER_MAXIMIZED_CAPTION_BUTTON: {
      return gfx::Size(kBrowserMaximizedCaptionButtonWidth[mode],
                       kBrowserMaximizedCaptionButtonHeight[mode]);
    }
    case AshLayoutSize::BROWSER_RESTORED_CAPTION_BUTTON: {
      return gfx::Size(kBrowserRestoredCaptionButtonWidth[mode],
                       kBrowserRestoredCaptionButtonHeight[mode]);
    }
    case AshLayoutSize::NON_BROWSER_CAPTION_BUTTON: {
      return gfx::Size(kNonBrowserCaptionButtonWidth[mode],
                       kNonBrowserCaptionButtonHeight[mode]);
    }
  }

  NOTREACHED();
  return gfx::Size();
}
