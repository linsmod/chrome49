// WebViewImplUpdateListener.h  
#ifndef WEBVIEW_IMPL_UPDATE_LISTENER_H  
#define WEBVIEW_IMPL_UPDATE_LISTENER_H  

#include "web/WebViewImpl.h" // 假设这是 WebViewImpl 的声明  

namespace blink {  
    class WebViewImplUpdateListener {  
    public:  
        virtual ~WebViewImplUpdateListener() {} // 虚析构函数，用于多态删除  
        virtual void onUpdate(WebViewImpl* impl) = 0; // 纯虚函数，要求子类实现  
    };  
    BLINK_EXPORT void webkitSetWebViewImplUpdateListener(blink::WebViewImplUpdateListener* listener);
} // namespace blink  

#endif // WEBVIEW_IMPL_UPDATE_LISTENER_H