#ifndef myRunHelper_H
#define myRunHelper_H
#include "wtf/text/WTFString.h"
namespace blink {
    class WebViewImpl;
    class LocalFrame;
    class MyBlinkWK {
    public:
    virtual WebViewImpl* webViewImpl() const = 0;
    virtual LocalFrame* frame() const = 0;
    virtual void navigateTo(const String& url) = 0;
        virtual void forceFullCompositingUpdate() = 0;
        virtual void run() = 0;  // 纯虚函数
        virtual ~MyBlinkWK() {}
    };
    MyBlinkWK* Create();
}

#endif