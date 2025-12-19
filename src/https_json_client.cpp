#include "https_json_client.h"
#include <sstream>
#include <iostream>

HTTPSJsonClient::HTTPSJsonClient()
    : curl_(nullptr)
    , headers_(nullptr)
    , lastStatusCode_(0)
    , connectTimeout_(10L)
    , requestTimeout_(30L)
    , verifySSL_(true)
    , userAgent_("HTTPSJsonClient/1.0") {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

HTTPSJsonClient::~HTTPSJsonClient() {
    if (headers_) {
        curl_slist_free_all(headers_);
        headers_ = nullptr;
    }
    if (curl_) {
        curl_easy_cleanup(curl_);
        curl_ = nullptr;
    }
    curl_global_cleanup();
}

void HTTPSJsonClient::setConnectTimeout(long timeout) {
    connectTimeout_ = timeout;
}

void HTTPSJsonClient::setRequestTimeout(long timeout) {
    requestTimeout_ = timeout;
}

void HTTPSJsonClient::setVerifySSL(bool verify) {
    verifySSL_ = verify;
}

void HTTPSJsonClient::setUserAgent(const std::string& ua) {
    userAgent_ = ua;
}

void HTTPSJsonClient::setProxy(const std::string& proxy) {
    proxy_ = proxy;
}

void HTTPSJsonClient::setHeader(const std::string& key, const std::string& value) {
    customHeaders_[key] = value;
}

void HTTPSJsonClient::clearHeaders() {
    customHeaders_.clear();
    if (headers_) {
        curl_slist_free_all(headers_);
        headers_ = nullptr;
    }
}

size_t HTTPSJsonClient::writeCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t totalSize = size * nmemb;
    output->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

void HTTPSJsonClient::initCurl() {
    if (!curl_) {
        curl_ = curl_easy_init();
        if (!curl_) {
            lastError_ = "Failed to initialize CURL";
            throw std::runtime_error(lastError_);
        }
    }
}

void HTTPSJsonClient::setCommonOptions(CURL* curl, const std::string& url) {
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent_.c_str());
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, connectTimeout_);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, requestTimeout_);

    // SSL选项
    if (verifySSL_) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    } else {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }

    // 代理设置
    if (!proxy_.empty()) {
        curl_easy_setopt(curl, CURLOPT_PROXY, proxy_.c_str());
    }

    // 设置HTTP头
    if (headers_) {
        curl_slist_free_all(headers_);
        headers_ = nullptr;
    }

    for (const auto& header : customHeaders_) {
        std::string headerStr = header.first + ": " + header.second;
        headers_ = curl_slist_append(headers_, headerStr.c_str());
    }

    // 添加Accept头（表示期望接收JSON）
    headers_ = curl_slist_append(headers_, "Accept: application/json");

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers_);
}

std::string HTTPSJsonClient::performRequest(CURL* curl) {
    std::string response;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        lastError_ = curl_easy_strerror(res);
        return "";
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &lastStatusCode_);
    return response;
}

// URL编码
std::string HTTPSJsonClient::urlEncode(const std::string& str) {
    CURL* curl = curl_easy_init();
    if (curl) {
        char* encoded = curl_easy_escape(curl, str.c_str(), str.length());
        if (encoded) {
            std::string result(encoded);
            curl_free(encoded);
            curl_easy_cleanup(curl);
            return result;
        }
        curl_easy_cleanup(curl);
    }
    return str; // 如果编码失败，返回原始字符串
}

std::string HTTPSJsonClient::get(const std::string& url) {
    initCurl();
    setCommonOptions(curl_, url);
    return performRequest(curl_);
}

std::string HTTPSJsonClient::post(const std::string& url, const std::string& data) {
    initCurl();
    setCommonOptions(curl_, url);

    // POST请求特有设置
    curl_easy_setopt(curl_, CURLOPT_POST, 1L);
    curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, data.c_str());
    curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE, data.length());

    // 设置Content-Type为JSON
    headers_ = curl_slist_append(headers_, "Content-Type: application/json");
    curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers_);

    return performRequest(curl_);
}

std::string HTTPSJsonClient::getLastError() const {
    return lastError_;
}

long HTTPSJsonClient::getLastStatusCode() const {
    return lastStatusCode_;
}
