/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WebURL_h
#define WebURL_h

#include "WebCString.h"
#include "WebString.h"
#include "url/third_party/mozilla/url_parse.h"

#include "url/gurl.h"

namespace blink {

class KURL;

class WebURL {
public:
    ~WebURL()
    {
    }

    WebURL()
        : m_isValid(false)
    {
    }

    WebURL(const WebURL& url)
        : m_string(url.m_string)
        , m_parsed(url.m_parsed)
        , m_isValid(url.m_isValid)
    {
    }

    WebURL& operator=(const WebURL& url)
    {
        m_string = url.m_string;
        m_parsed = url.m_parsed;
        m_isValid = url.m_isValid;
        return *this;
    }

    const WebString& string() const
    {
        return m_string;
    }

    const url::Parsed& parsed() const
    {
        return m_parsed;
    }

    bool isValid() const
    {
        return m_isValid;
    }

    bool isEmpty() const
    {
        return m_string.isEmpty();
    }

    bool isNull() const
    {
        return m_string.isEmpty();
    }

    BLINK_PLATFORM_EXPORT WebURL(const KURL&);
    BLINK_PLATFORM_EXPORT WebURL& operator=(const KURL&);
    BLINK_PLATFORM_EXPORT operator KURL() const;
    WebURL(const GURL& url)
        : m_string(WebString::fromUTF8(url.possibly_invalid_spec()))
        , m_parsed(url.parsed_for_possibly_invalid_spec())
        , m_isValid(url.is_valid())
    {
    }

    WebURL& operator=(const GURL& url)
    {
        m_string = WebString::fromUTF8(url.possibly_invalid_spec());
        m_parsed = url.parsed_for_possibly_invalid_spec();
        m_isValid = url.is_valid();
        return *this;
    }

    operator GURL() const
    {
        return isNull() ? GURL() : GURL(m_string.utf8(), m_parsed, m_isValid);
    }

private:
    WebString m_string;
    url::Parsed m_parsed;
    bool m_isValid;
};

inline bool operator==(const WebURL& a, const WebURL& b)
{
    return a.string().equals(b.string());
}

inline bool operator!=(const WebURL& a, const WebURL& b)
{
    return !(a == b);
}

} // namespace blink

#endif
