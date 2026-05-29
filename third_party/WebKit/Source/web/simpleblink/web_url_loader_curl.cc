#include "web/simpleblink/web_url_loader_curl.h"

#include <curl/curl.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <algorithm>
#include <cstdio>

#include "base/bind.h"
#include "base/location.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/WebURLLoaderClient.h"
#include "public/platform/WebURLResponse.h"
#include "public/platform/WebURLError.h"
#include "public/platform/WebData.h"
#include "public/platform/WebString.h"
#include "public/platform/WebHTTPHeaderVisitor.h"
#include "platform/weborigin/KURL.h"
#include "platform/network/EncodedFormData.h"

using namespace blink;

namespace html_viewer {

// ---- 辅助数据结构 for async curl tasks ----
// Chrome 49's base::Bind doesn't support lambdas, so we use raw pointers.
struct CurlTaskData {
    std::shared_ptr<bool> alive;
    std::shared_ptr<CurlResult> result;
    scoped_refptr<base::SingleThreadTaskRunner> mainRunner;
    WebURLLoaderClient* client;
    WebURLRequest reqCopy;
};

// Shared background thread for all curl downloads.
static base::Thread* g_curlThread = nullptr;
static base::Thread* getCurlThread() {
    if (!g_curlThread) {
        g_curlThread = new base::Thread("CurlWorker");
        g_curlThread->Start();
    }
    return g_curlThread;
}

// Free function: deliver curl result safely to Blink.
// Only uses client if *alive is still true (meaning no cancel/destroy happened).
static void deliverCurlResult(std::shared_ptr<bool> alive,
    std::shared_ptr<CurlResult> result,
    WebURLLoaderClient* client) {
    if (!*alive || !client)
        return;

    if (!result->success) {
        WebURLError error;
        error.reason = -1;
        error.domain = WebString::fromUTF8(result->errorMsg);
        client->didFail(nullptr, error);
        return;
    }

    WebURLResponse response;
    response.initialize();
    response.setURL(KURL(ParsedURLString, WebString::fromUTF8(result->finalUrl)));
    response.setHTTPStatusCode(result->statusCode);
    response.setHTTPStatusText(WebString::fromUTF8(result->statusText));
    if (!result->mimeType.empty())
        response.setMIMEType(WebString::fromUTF8(result->mimeType));
    if (!result->charset.empty())
        response.setTextEncodingName(WebString::fromUTF8(result->charset));

    client->didReceiveResponse(nullptr, response);
    if (!*alive)
        return;
    if (!result->data.empty())
        client->didReceiveData(nullptr, result->data.data(), result->data.size(), -1);
    if (!*alive)
        return;
    client->didFinishLoading(nullptr, 0.0, result->data.size());
}

// Delete CurlTaskData on the main thread (WebURLRequest/WebString dtors
// touch AtomicString tables which are main-thread-only).
static void deleteCurlTaskData(CurlTaskData* d) {
    delete d;
}

static void runCurlTask(CurlTaskData* d) {
    // Create a temporary loader just for the curl download.
    // We can't use d->self anymore because the WebURLLoaderCurl might be deleted.
    WebURLLoaderCurl* tempLoader = new WebURLLoaderCurl();
    tempLoader->loadWithCurl(d->reqCopy, nullptr, d->result.get());
    delete tempLoader;

    // Always deliver AND delete on main thread (AtomicString safety).
    d->mainRunner->PostTask(FROM_HERE, base::Bind(
        &deliverCurlResult, d->alive, d->result, d->client
    ));
    d->mainRunner->PostTask(FROM_HERE, base::Bind(
        &deleteCurlTaskData, d
    ));
}

// ---- helpers ----

static String extensionToMime(const String& ext) {
    String lower = ext.lower();
    if (lower == "html" || lower == "htm")  return "text/html";
    if (lower == "css")                     return "text/css";
    if (lower == "js")                      return "application/javascript";
    if (lower == "png")                     return "image/png";
    if (lower == "jpg" || lower == "jpeg")  return "image/jpeg";
    if (lower == "gif")                     return "image/gif";
    if (lower == "svg")                     return "image/svg+xml";
    if (lower == "txt")                     return "text/plain";
    if (lower == "json")                    return "application/json";
    if (lower == "xml")                     return "application/xml";
    if (lower == "pdf")                     return "application/pdf";
    if (lower == "webp")                    return "image/webp";
    if (lower == "ico")                     return "image/x-icon";
    return "application/octet-stream";
}

static unsigned char b64Reverse(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return 0xFF;
}

static Vector<char> base64Decode(const char* data, size_t len) {
    Vector<char> out;
    out.resize((len * 3) / 4 + 1);
    size_t pos = 0;
    for (size_t i = 0; i < len; i += 4) {
        unsigned char b[4] = {0xFF, 0xFF, 0xFF, 0xFF};
        int valid = 0;
        for (int j = 0; j < 4 && i + j < len; ++j) {
            unsigned char c = b64Reverse((unsigned char)data[i + j]);
            if (c != 0xFF) { b[j] = c; ++valid; }
        }
        if (valid >= 2) out.data()[pos++] = (b[0] << 2) | (b[1] >> 4);
        if (valid >= 3) out.data()[pos++] = (b[1] << 4) | (b[2] >> 2);
        if (valid >= 4) out.data()[pos++] = (b[2] << 6) | b[3];
    }
    out.resize(pos);
    return out;
}

static String getLocalFilePath(const KURL& url) {
    String path = url.path();
    if (path.length() > 0 && path[0] == '/')
        return path;
    return url.string();
}

static String getFileExtension(const String& path) {
    int dot = path.reverseFind('.');
    if (dot != -1)
        return path.substring(dot + 1);
    return "";
}

// ---- WebURLLoaderCurl ----

WebURLLoaderCurl::WebURLLoaderCurl() {
}

WebURLLoaderCurl::~WebURLLoaderCurl() {
    if (m_alive)
        *m_alive = false;
    m_alive.reset();
}

void WebURLLoaderCurl::loadSynchronously(const WebURLRequest& request,
    WebURLResponse& response, WebURLError& error, WebData& data) {
    KURL url = request.url();

    if (url.isLocalFile()) {
        String path = getLocalFilePath(url);
        FILE* f = fopen(path.utf8().data(), "rb");
        if (!f) {
            error.reason = errno;
            error.domain = WebString::fromUTF8("file");
            return;
        }
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);

        Vector<char> buf;
        buf.resize(size);
        fread(buf.data(), 1, size, f);
        fclose(f);

        response.initialize();
        response.setURL(url);
        response.setHTTPStatusCode(200);
        response.setHTTPStatusText(WebString::fromUTF8("OK"));

        String ext = getFileExtension(path);
        if (!ext.isEmpty())
            response.setMIMEType(WebString(extensionToMime(ext)));
        else
            response.setMIMEType(WebString::fromUTF8("text/html"));

        data.assign(buf.data(), buf.size());
        return;
    }

    CurlResult result;
    loadWithCurl(request, nullptr, &result);

    if (!result.success) {
        error.reason = -1;
        error.domain = WebString::fromUTF8(result.errorMsg);
        return;
    }

    response.initialize();
    response.setURL(KURL(ParsedURLString, WebString::fromUTF8(result.finalUrl)));
    response.setHTTPStatusCode(result.statusCode);
    response.setHTTPStatusText(WebString::fromUTF8(result.statusText));
    if (!result.mimeType.empty())
        response.setMIMEType(WebString::fromUTF8(result.mimeType));
    if (!result.charset.empty())
        response.setTextEncodingName(WebString::fromUTF8(result.charset));

    data.assign(result.data.data(), result.data.size());
}

void WebURLLoaderCurl::loadAsynchronously(const WebURLRequest& request,
    WebURLLoaderClient* client) {
    m_client = client;
    m_cancelled = false;
    m_hasResponse = false;
    m_mainThreadRunner = base::ThreadTaskRunnerHandle::Get();
    m_alive = std::make_shared<bool>(true);

    KURL url = request.url();

    if (url.isLocalFile()) {
        fileLoadImpl(url);
        return;
    }

    if (url.protocolIsData()) {
        String urlStr = url.string();
        String dataStr = urlStr.substring(5);
        int comma = dataStr.find(',');
        if (comma == -1) {
            if (m_client)
                m_client->didFail(this, WebURLError());
            return;
        }

        String mediaType = dataStr.substring(0, comma);
        String rawData = dataStr.substring(comma + 1);
        bool isBase64 = mediaType.endsWith(";base64", TextCaseInsensitive);
        if (isBase64)
            mediaType = mediaType.substring(0, mediaType.length() - 7);

        String mimeType = mediaType.isEmpty() ? "text/plain" : mediaType;
        String charset;
        int charsetPos = mimeType.find("charset=");
        if (charsetPos != -1)
            charset = mimeType.substring(charsetPos + 8);
        int semi = mimeType.find(';');
        if (semi != -1)
            mimeType = mimeType.substring(0, semi);

        Vector<char> decoded;
        std::string rawUtf8 = rawData.utf8().data();
        if (isBase64)
            decoded = base64Decode(rawUtf8.data(), rawUtf8.size());
        else
            decoded.append(rawUtf8.data(), rawUtf8.size());

        WebURLResponse response;
        response.initialize();
        response.setURL(url);
        response.setHTTPStatusCode(200);
        response.setMIMEType(WebString::fromUTF8(mimeType.utf8().data()));
        if (!charset.isEmpty())
            response.setTextEncodingName(WebString(charset));

        if (m_client) {
            m_client->didReceiveResponse(this, response);
            if (!decoded.isEmpty())
                m_client->didReceiveData(this, decoded.data(), decoded.size(), -1);
            m_client->didFinishLoading(this, 0.0, decoded.size());
        }
        return;
    }

    // HTTP/HTTPS - use shared background thread for curl
    base::Thread* curlThread = getCurlThread();

    CurlTaskData* d = new CurlTaskData();
    d->client = client;
    d->alive = m_alive;
    d->result = std::make_shared<CurlResult>();
    d->mainRunner = m_mainThreadRunner;
    d->reqCopy = request;

    curlThread->message_loop()->task_runner()->PostTask(FROM_HERE,
        base::Bind(&runCurlTask, d));
}

void WebURLLoaderCurl::cancel() {
    m_cancelled = true;
    m_client = nullptr;
    if (m_alive)
        *m_alive = false;
}

void WebURLLoaderCurl::setDefersLoading(bool) {
}

void WebURLLoaderCurl::didChangePriority(WebURLRequest::Priority, int) {
}

bool WebURLLoaderCurl::attachThreadedDataReceiver(
    WebThreadedDataReceiver*) {
    return false;
}

void WebURLLoaderCurl::setLoadingTaskRunner(WebTaskRunner*) {
}

void WebURLLoaderCurl::fileLoadImpl(const KURL& url) {
    String path = getLocalFilePath(url);
    struct stat st;
    if (stat(path.utf8().data(), &st) != 0) {
        if (m_client) {
            WebURLError error;
            error.reason = errno;
            error.domain = WebString::fromUTF8("file");
            m_client->didFail(this, error);
        }
        return;
    }

    int fd = open(path.utf8().data(), O_RDONLY);
    if (fd < 0) {
        if (m_client) {
            WebURLError error;
            error.reason = errno;
            error.domain = WebString::fromUTF8("file");
            m_client->didFail(this, error);
        }
        return;
    }

    WebURLResponse response;
    response.initialize();
    response.setURL(url);
    response.setHTTPStatusCode(200);
    response.setHTTPStatusText(WebString::fromUTF8("OK"));

    String ext = getFileExtension(path);
    if (!ext.isEmpty())
        response.setMIMEType(WebString(extensionToMime(ext)));
    else
        response.setMIMEType(WebString::fromUTF8("text/html"));

    if (m_client)
        m_client->didReceiveResponse(this, response);

    char buffer[8192];
    ssize_t bytesRead;
    int64_t total = 0;
    while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0) {
        if (m_client && !m_cancelled)
            m_client->didReceiveData(this, buffer, bytesRead, -1);
        total += bytesRead;
    }

    close(fd);

    if (m_client && !m_cancelled)
        m_client->didFinishLoading(this, 0.0, total);
}

class CurlHeaderCopier : public WebHTTPHeaderVisitor {
public:
    CurlHeaderCopier(struct curl_slist** h) : m_headers(h) {}
    void visitHeader(const WebString& name, const WebString& value) override {
        std::string line = name.utf8().data() + std::string(": ") + value.utf8().data();
        *m_headers = curl_slist_append(*m_headers, line.c_str());
    }
    struct curl_slist** m_headers;
};

void WebURLLoaderCurl::loadWithCurl(const WebURLRequest& request,
    WebURLLoaderClient* client, CurlResult* result) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        result->success = false;
        result->errorMsg = "curl_easy_init failed";
        return;
    }

    std::string url = request.url().string().utf8().data();
    std::string method = request.httpMethod().utf8().data();
    if (method.empty())
        method = "GET";

    std::string userAgent = Platform::current()->userAgent().utf8().data();

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, result);

    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, &headerCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, result);

    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        if (!request.httpBody().isNull()) {
            PassRefPtr<EncodedFormData> formData = request.httpBody();
            Vector<char> flattened;
            formData->flatten(flattened);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, flattened.data());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)flattened.size());
        }
    }

    struct curl_slist* headers = nullptr;
    CurlHeaderCopier copier(&headers);
    request.visitHTTPHeaderFields(&copier);
    if (headers)
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);

    if (res == 0) {
        result->success = true;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result->statusCode);
        char* effUrl = nullptr;
        curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effUrl);
        if (effUrl) result->finalUrl = effUrl;
    } else {
        result->errorMsg = curl_easy_strerror(res);
    }

    if (headers)
        curl_slist_free_all(headers);

    if (result->statusCode == 200) result->statusText = "OK";
    else if (result->statusCode == 301) result->statusText = "Moved Permanently";
    else if (result->statusCode == 302) result->statusText = "Found";
    else if (result->statusCode == 304) result->statusText = "Not Modified";
    else if (result->statusCode == 400) result->statusText = "Bad Request";
    else if (result->statusCode == 401) result->statusText = "Unauthorized";
    else if (result->statusCode == 403) result->statusText = "Forbidden";
    else if (result->statusCode == 404) result->statusText = "Not Found";
    else if (result->statusCode == 500) result->statusText = "Internal Server Error";
    else if (result->statusCode == 502) result->statusText = "Bad Gateway";
    else if (result->statusCode == 503) result->statusText = "Service Unavailable";
    else result->statusText = "Unknown";

    curl_easy_cleanup(curl);
}

void WebURLLoaderCurl::dispatchResultOnMainThread(std::shared_ptr<CurlResult> result) {
    if (m_cancelled || !m_client)
        return;

    if (!result->success) {
        WebURLError error;
        error.reason = -1;
        error.domain = WebString::fromUTF8(result->errorMsg);
        m_client->didFail(this, error);
        return;
    }

    WebURLResponse response;
    response.initialize();
    response.setURL(KURL(ParsedURLString, WebString::fromUTF8(result->finalUrl)));
    response.setHTTPStatusCode(result->statusCode);
    response.setHTTPStatusText(WebString::fromUTF8(result->statusText));
    if (!result->mimeType.empty())
        response.setMIMEType(WebString::fromUTF8(result->mimeType));
    if (!result->charset.empty())
        response.setTextEncodingName(WebString::fromUTF8(result->charset));

    m_client->didReceiveResponse(this, response);

    if (!result->data.empty())
        m_client->didReceiveData(this, result->data.data(), result->data.size(), -1);

    m_client->didFinishLoading(this, 0.0, result->data.size());
}

size_t WebURLLoaderCurl::writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    auto* result = static_cast<CurlResult*>(userp);
    result->data.insert(result->data.end(), (char*)contents, (char*)contents + total);
    return total;
}

size_t WebURLLoaderCurl::headerCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    auto* result = static_cast<CurlResult*>(userp);
    std::string header((char*)contents, total);

    if (header.find("HTTP/") == 0)
        return total;

    std::string lower(header.size(), '\0');
    std::transform(header.begin(), header.end(), lower.begin(), ::tolower);

    if (lower.find("content-type:") == 0) {
        std::string value = header.substr(13);
        size_t start = value.find_first_not_of(" \t\r\n");
        if (start != std::string::npos) {
            value = value.substr(start);
            size_t charsetPos = value.find("charset=");
            if (charsetPos != std::string::npos) {
                std::string charset = value.substr(charsetPos + 8);
                size_t semi = charset.find(';');
                if (semi != std::string::npos)
                    charset = charset.substr(0, semi);
                charset.erase(0, charset.find_first_not_of(" \t\"'"));
                charset.erase(charset.find_last_not_of(" \t\"'\r\n") + 1);
                result->charset = charset;
            }
            size_t semi = value.find(';');
            if (semi != std::string::npos)
                value = value.substr(0, semi);
            result->mimeType = value;
        }
    }

    return total;
}

} // namespace html_viewer
