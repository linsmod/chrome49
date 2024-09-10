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

#include "public/platform/Platform.h"
#include "public/web/WebKit.h"
#include "web/tests/WebUnitTests.h"
#include <content/test/blink_test_environment.h>
#include <cstdio>
// #include "web/PageOverlay.h"

#include "ui/aura/env.h"
#include "base/command_line.h"
namespace {

// Test helpers to support the fact that blink tests are gloriously complicated
// in a shared library build. See WebUnitTests.h for more details.
base::MessageLoop* message_loop;
void preTestHook()
{
    message_loop = content::SetUpBlinkTestEnvironmentAndGetMainMessageLoop();
    // blink::PageOverlayTest_copy test;
    // test.runPageOverlayTestWithAcceleratedCompositing();
}

void postTestHook()
{
    // 使用 out/Default/webkit_unit_tests --single-process-tests
    // 否则我们添加的loop->Run()会导致test框架的频繁重试
    message_loop->Run();
    content::TearDownBlinkTestEnvironment();
}

} // namespace

//  class MyWebViewUpdateListener : public blink::WebViewImplUpdateListener {  
//     public:  
//         void onUpdate(blink::WebViewImpl* impl) override {  
//              printf("MyWebViewUpdateListener::onUpdate");
//         }  
//     };  

int main(int argc, char** argv)
{
    base::CommandLine::Init(argc, argv);
    aura::Env::CreateInstance(true);
    return blink::runWebTests(argc, argv, &preTestHook, &postTestHook);
}
