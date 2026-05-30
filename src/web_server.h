#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <map>
#include <mutex>
#include <string>
#include <vector>
#include "crow/crow.h"
#include "json_parser.h"

class WebServer {
private:
    std::map<std::string, std::vector<VideoInfo>> videoList;
    mutable std::mutex videoListMutex;
    std::map<std::string, int> siteFailureCounts;
    mutable std::mutex siteFailureCountsMutex;
    crow::SimpleApp app;

    static const std::string INPUT_PATH;
    static const std::string OUTPUT_PATH;

    bool shouldSkipSite(const std::string& domain, int maxFailures) const;
    int recordSiteRequestResult(const std::string& domain, bool requestSucceeded);

public:
    WebServer() = default;
    ~WebServer() = default;

    // 启动Web服务器
    void run(int port = 8080);

    // 设置视频数据
    void setVideoList(const std::map<std::string, std::vector<VideoInfo>>& data);

    // 读取视频数据
    std::map<std::string, std::vector<VideoInfo>> getVideoList();

    // 首页路由处理
    void setupRoutes();

    bool search(const std::string& key);
    bool updateSiteConfig();

    json readSiteConfig(const std::string& filePath);

    bool deleteOutputJsonFiles();
};

#endif // WEBSERVER_H
