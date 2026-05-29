#ifndef WEB_URL_LOADER_CURL_H_
#define WEB_URL_LOADER_CURL_H_

#include "public/platform/WebURLLoader.h"
#include "base/memory/ref_counted.h"
#include <string>
#include <vector>
#include <memory>

namespace blink {
class WebURLRequest;
class WebURLLoaderClient;
class WebURLResponse;
class WebData;
class KURL;
class WebTaskRunner;
}

namespace base {
class SingleThreadTaskRunner;
class Thread;
}

namespace html_viewer {

struct CurlResult {
    std::vector<char> data;
    std::string mimeType;
    std::string charset;
    std::string statusText;
    std::string finalUrl;
    long statusCode = 0;
    bool success = false;
    std::string errorMsg;
};

class WebURLLoaderCurl : public blink::WebURLLoader {
public:
    WebURLLoaderCurl();
    ~WebURLLoaderCurl() override;

    void loadSynchronously(const blink::WebURLRequest& request,
        blink::WebURLResponse& response, blink::WebURLError& error,
        blink::WebData& data) override;
    void loadAsynchronously(const blink::WebURLRequest& request,
        blink::WebURLLoaderClient* client) override;
    void cancel() override;
    void setDefersLoading(bool value) override;
    void didChangePriority(blink::WebURLRequest::Priority newPriority, int intraPriorityValue) override;
    bool attachThreadedDataReceiver(blink::WebThreadedDataReceiver*) override;
    void setLoadingTaskRunner(blink::WebTaskRunner*) override;

    // Internal - public because called from static callback
    void loadWithCurl(const blink::WebURLRequest& request,
        blink::WebURLLoaderClient* client, CurlResult* result);
    void dispatchResultOnMainThread(std::shared_ptr<CurlResult> result);

private:
    void fileLoadImpl(const blink::KURL& url);

    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp);
    static size_t headerCallback(void* contents, size_t size, size_t nmemb, void* userp);

    blink::WebURLLoaderClient* m_client = nullptr;
    std::string m_url;
    bool m_cancelled = false;
    bool m_hasResponse = false;

    scoped_refptr<base::SingleThreadTaskRunner> m_mainThreadRunner;
    std::shared_ptr<bool> m_alive;
};

} // namespace html_viewer

#endif // WEB_URL_LOADER_CURL_H_
