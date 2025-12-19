// https_json_client.h
#ifndef HTTPS_JSON_CLIENT_H
#define HTTPS_JSON_CLIENT_H

#include <string>
#include <map>
#include <memory>
#include <curl/curl.h>

class HTTPSJsonClient {
public:
    // 构造函数
    HTTPSJsonClient();
    // 析构函数
    ~HTTPSJsonClient();

    // 禁用拷贝和赋值
    HTTPSJsonClient(const HTTPSJsonClient&) = delete;
    HTTPSJsonClient& operator=(const HTTPSJsonClient&) = delete;

    // 配置选项
    void setConnectTimeout(long timeout);      // 连接超时（秒）
    void setRequestTimeout(long timeout);      // 请求超时（秒）
    void setVerifySSL(bool verify);            // 是否验证SSL
    void setUserAgent(const std::string& ua);  // 设置User-Agent
    void setProxy(const std::string& proxy);   // 设置代理

    // 设置自定义HTTP头
    void setHeader(const std::string& key, const std::string& value);
    void clearHeaders();

    // 执行GET请求
    std::string get(const std::string& url);

    // 执行POST请求
    std::string post(const std::string& url, const std::string& data);

    // 获取最后一次错误信息
    std::string getLastError() const;

    // 获取最后一次HTTP状态码
    long getLastStatusCode() const;

    // URL编码
    std::string urlEncode(const std::string& str);

private:
    // 静态回调函数
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* output);

    // 初始化CURL
    void initCurl();

    // 设置公共选项
    void setCommonOptions(CURL* curl, const std::string& url);

    // 执行请求
    std::string performRequest(CURL* curl);

private:
    CURL* curl_;
    std::string lastError_;
    long lastStatusCode_;
    struct curl_slist* headers_;

    // 配置选项
    long connectTimeout_;
    long requestTimeout_;
    bool verifySSL_;
    std::string userAgent_;
    std::string proxy_;

    // HTTP头
    std::map<std::string, std::string> customHeaders_;
};

#endif // HTTPS_JSON_CLIENT_H
