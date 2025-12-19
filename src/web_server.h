#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <map>
#include <string>
#include <vector>
#include "crow/crow.h"
#include "json_parser.h"

class WebServer {
private:
    std::map<std::string, std::vector<VideoInfo>> videoList;
    crow::SimpleApp app;

    static const std::string INPUT_PATH;
    static const std::string OUTPUT_PATH;

public:
    WebServer();
    ~WebServer();

    // 启动Web服务器
    void run(int port = 8080);

    // 设置视频数据
    void setVideoList(const std::map<std::string, std::vector<VideoInfo>>& data);

    // 读取视频数据
    std::map<std::string, std::vector<VideoInfo>> getVideoList();

    // 首页路由处理
    void setupRoutes();

    bool search(const std::string& key);

    // 读取站点配置的方法
    json readSiteConfig(const std::string& filePath);

    // 删除JSON文件的方法
    bool deleteOutputJsonFiles();
};

#endif // WEBSERVER_H
