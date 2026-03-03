#
# Copyright (C) 2016 The Chromium Authors. All rights reserved.
#
# HTMLViewer GYP 构建文件
# 基于 Blink Web Layer 为 GLAPP 提供 HTML 渲染能力
#

{
    'variables': {
    },
    'includes': [
        '../bindings/bindings.gypi',
        '../core/core.gypi',
        '../build/features.gypi',
        '../build/scripts/scripts.gypi',
        '../build/win/precompile.gypi',
        '../modules/modules.gypi',
        '../platform/blink_platform.gypi',
        '../wtf/wtf.gypi',
    ],
    'targets': [
        # HTMLViewer 静态库
        {
            'target_name': 'blink_html_viewer',
            'type': 'static_library',
            'dependencies': [
                '../config.gyp:config',
                '../platform/blink_platform.gyp:blink_common',
                '../modules/modules.gyp:modules',
                '<(DEPTH)/skia/skia.gyp:skia',
                '<(angle_path)/src/angle.gyp:translator',
                '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
                '<(DEPTH)/ui/gfx/gfx.gyp:gfx_geometry',
                '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
            ],
            'include_dirs': [
                '<(DEPTH)/third_party/WebKit',
                '<(DEPTH)/third_party/WebKit/Source',
                '<(DEPTH)/third_party/WebKit/Source/web',
                '<(DEPTH)/third_party/WebKit/Source/web/tests',
                '<(DEPTH)/third_party/WebKit/Source/core',
                '<(DEPTH)/third_party/WebKit/Source/platform',
                '<(DEPTH)/third_party/WebKit/Source/platform/testing',
                '<(DEPTH)/third_party/WebKit/Source/wtf',
                '<(DEPTH)/third_party/WebKit/public',
                '<(DEPTH)/third_party/WebKit/public/web',
                '<(DEPTH)/third_party/WebKit/public/platform',
                '<(angle_path)/include',
                '<(DEPTH)/third_party/skia/include/core',
                '<(DEPTH)/third_party/skia/include/utils',
            ],
            'defines': [
                'BLINK_IMPLEMENTATION=1',
                'INSIDE_BLINK',
            ],
            'sources': [
                'blink_web_wrapper.h',
                'blink_web_wrapper.cpp',
            ],
            'conditions': [
                # 在组件构建中添加 SimTest 支持文件
                ['component=="shared_library"', {
                    'sources': [
                        '../tests/sim/SimCompositor.cpp',
                        '../tests/sim/SimNetwork.cpp',
                        '../tests/sim/SimWebViewClient.cpp',
                        '../tests/sim/SimDisplayItemList.cpp',
                    ],
                }],
            ],
        },
    ],
}
