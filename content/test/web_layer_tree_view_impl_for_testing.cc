// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/web_layer_tree_view_impl_for_testing.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "base/thread_task_runner_handle.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_timeline.h"
#include "cc/base/switches.h"
#include "cc/blink/web_compositor_animation_timeline_impl.h"
#include "cc/blink/web_layer_impl.h"
#include "cc/input/input_handler.h"
#include "cc/layers/layer.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/trees/layer_tree_host.h"
#include "content/test/test_blink_web_unit_test_support.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebLayer.h"
#include "third_party/WebKit/public/platform/WebLayerTreeView.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"

// linsmod Add includes
#include "ui/gl/test/gl_surface_test_support.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/test/context_factories_for_test.h"
#include "ui/aura/env.h"
using blink::WebColor;
using blink::WebRect;
using blink::WebSize;

namespace content {

WebLayerTreeViewImplForTesting::WebLayerTreeViewImplForTesting() {}

WebLayerTreeViewImplForTesting::~WebLayerTreeViewImplForTesting() {}

// linsmod Add fields
scoped_ptr<ui::Compositor> compositor_;

void WebLayerTreeViewImplForTesting::Initialize() {
  
  if(aura::Env::GetInstance()->compositor()){
    compositor_.reset(aura::Env::GetInstance()->compositor());
  }
  else{
    // cc::LayerTreeSettings settings;

    // // For web contents, layer transforms should scale up the contents of layers
    // // to keep content always crisp when possible.
    // settings.layer_transforms_should_scale_layer_contents = true;

    // // Accelerated animations are enabled for unit tests.
    // settings.accelerated_animation_enabled = true;
    // // External cc::AnimationHost is enabled for unit tests.
    // settings.use_compositor_animation_timelines = true;

    // cc::LayerTreeHost::InitParams params;
    // params.client = this;
    // params.settings = &settings;
    // params.main_task_runner = base::ThreadTaskRunnerHandle::Get();
    // params.task_graph_runner = task_graph_runner_;

    // linsmod comment out
    // layer_tree_host_ = cc::LayerTreeHost::CreateSingleThreaded(this, &params);
    // DCHECK(layer_tree_host_);

    // linsmod Add
    gfx::GLSurfaceTestSupport::InitializeOneOff();
    bool enable_pixel_output = false;
    ui::ContextFactory* context_factory =
        ui::InitializeContextFactoryForTests(enable_pixel_output);
    compositor_.reset(
        new ui::Compositor(context_factory, base::ThreadTaskRunnerHandle::Get()));
    compositor_->SetAcceleratedWidget(gfx::kNullAcceleratedWidget);
  }
  layer_tree_host_ = make_scoped_ptr( compositor_->GetLayerTreeHost());
}

void WebLayerTreeViewImplForTesting::setRootLayer(
    const blink::WebLayer& root) {
  layer_tree_host_->SetRootLayer(
      static_cast<const cc_blink::WebLayerImpl*>(&root)->layer());
}

void WebLayerTreeViewImplForTesting::clearRootLayer() {
  layer_tree_host_->SetRootLayer(scoped_refptr<cc::Layer>());
}

void WebLayerTreeViewImplForTesting::attachCompositorAnimationTimeline(
    blink::WebCompositorAnimationTimeline* compositor_timeline) {
  DCHECK(compositor_timeline);
  DCHECK(layer_tree_host_->animation_host());
  layer_tree_host_->animation_host()->AddAnimationTimeline(
      static_cast<const cc_blink::WebCompositorAnimationTimelineImpl*>(
          compositor_timeline)
          ->animation_timeline());
}

void WebLayerTreeViewImplForTesting::detachCompositorAnimationTimeline(
    blink::WebCompositorAnimationTimeline* compositor_timeline) {
  DCHECK(compositor_timeline);
  DCHECK(layer_tree_host_->animation_host());
  layer_tree_host_->animation_host()->RemoveAnimationTimeline(
      static_cast<const cc_blink::WebCompositorAnimationTimelineImpl*>(
          compositor_timeline)
          ->animation_timeline());
}

void WebLayerTreeViewImplForTesting::setViewportSize(
    const WebSize& unused_deprecated,
    const WebSize& device_viewport_size) {
  layer_tree_host_->SetViewportSize(device_viewport_size);
}

void WebLayerTreeViewImplForTesting::setViewportSize(
    const WebSize& device_viewport_size) {
  layer_tree_host_->SetViewportSize(device_viewport_size);
}

void WebLayerTreeViewImplForTesting::setDeviceScaleFactor(
    float device_scale_factor) {
  layer_tree_host_->SetDeviceScaleFactor(device_scale_factor);
}

void WebLayerTreeViewImplForTesting::setBackgroundColor(WebColor color) {
  layer_tree_host_->set_background_color(color);
}

void WebLayerTreeViewImplForTesting::setHasTransparentBackground(
    bool transparent) {
  layer_tree_host_->set_has_transparent_background(transparent);
}

void WebLayerTreeViewImplForTesting::setVisible(bool visible) {
  layer_tree_host_->SetVisible(visible);
}

void WebLayerTreeViewImplForTesting::setPageScaleFactorAndLimits(
    float page_scale_factor,
    float minimum,
    float maximum) {
  layer_tree_host_->SetPageScaleFactorAndLimits(
      page_scale_factor, minimum, maximum);
}

void WebLayerTreeViewImplForTesting::startPageScaleAnimation(
    const blink::WebPoint& scroll,
    bool use_anchor,
    float new_page_scale,
    double duration_sec) {}

void WebLayerTreeViewImplForTesting::setNeedsAnimate() {
  layer_tree_host_->SetNeedsAnimate();
}

void WebLayerTreeViewImplForTesting::didStopFlinging() {}

void WebLayerTreeViewImplForTesting::setDeferCommits(bool defer_commits) {
  layer_tree_host_->SetDeferCommits(defer_commits);
}

void WebLayerTreeViewImplForTesting::UpdateLayerTreeHost() {
  // @linsmod
  layer_tree_host_->client()->UpdateLayerTreeHost();
}

void WebLayerTreeViewImplForTesting::ApplyViewportDeltas(
    const gfx::Vector2dF& inner_delta,
    const gfx::Vector2dF& outer_delta,
    const gfx::Vector2dF& elastic_overscroll_delta,
    float page_scale,
    float top_controls_delta) {
}

void WebLayerTreeViewImplForTesting::RequestNewOutputSurface() {
  // Intentionally do not create and set an OutputSurface.

  // linsmod Add
  // 不需要自己创建output_surface_了，
  // 直接用compositor_来管理
  compositor_->RequestNewOutputSurface();
}

void WebLayerTreeViewImplForTesting::DidFailToInitializeOutputSurface() {
  NOTREACHED();

  // linsmod Add
  RequestNewOutputSurface();
}

void WebLayerTreeViewImplForTesting::registerForAnimations(
    blink::WebLayer* layer) {
  cc::Layer* cc_layer = static_cast<cc_blink::WebLayerImpl*>(layer)->layer();
  cc_layer->RegisterForAnimations(layer_tree_host_->animation_registrar());

  // @linsmod
  // WebViewImpl在初始化之后，对它的ViewPort的Resize会导致执行updateAllLifecyclePhases
  // 直到运行到这里。
  // 由于是是测试代码，它并不会设置cc_layer的layer_tree_host，必须在创建cc_layer的步骤中设置它
  // 在这里设置是一个比较好的位置

  cc_layer->SetLayerTreeHost(layer_tree_host_.get());
}

void WebLayerTreeViewImplForTesting::registerViewportLayers(
    const blink::WebLayer* overscrollElasticityLayer,
    const blink::WebLayer* pageScaleLayer,
    const blink::WebLayer* innerViewportScrollLayer,
    const blink::WebLayer* outerViewportScrollLayer) {
  layer_tree_host_->RegisterViewportLayers(
      // The scroll elasticity layer will only exist when using pinch virtual
      // viewports.
      overscrollElasticityLayer
          ? static_cast<const cc_blink::WebLayerImpl*>(
                overscrollElasticityLayer)->layer()
          : NULL,
      static_cast<const cc_blink::WebLayerImpl*>(pageScaleLayer)->layer(),
      static_cast<const cc_blink::WebLayerImpl*>(innerViewportScrollLayer)
          ->layer(),
      // The outer viewport layer will only exist when using pinch virtual
      // viewports.
      outerViewportScrollLayer
          ? static_cast<const cc_blink::WebLayerImpl*>(outerViewportScrollLayer)
                ->layer()
          : NULL);
}

void WebLayerTreeViewImplForTesting::clearViewportLayers() {
  layer_tree_host_->RegisterViewportLayers(scoped_refptr<cc::Layer>(),
                                           scoped_refptr<cc::Layer>(),
                                           scoped_refptr<cc::Layer>(),
                                           scoped_refptr<cc::Layer>());
}

void WebLayerTreeViewImplForTesting::registerSelection(
    const blink::WebSelection& selection) {
}

void WebLayerTreeViewImplForTesting::clearSelection() {
}

}  // namespace content
