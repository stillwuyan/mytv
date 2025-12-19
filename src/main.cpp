#include <unistd.h>
#include <iostream>
#include <string>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <vector>
#include <nlohmann/json.hpp>
#include "json_parser.h"
#include "https_json_client.h"
#include "web_server.h"

using json = nlohmann::json;

// 检查文件是否存在并解析JSON文件
bool parseJsonFile(const std::string& filePath, json& data) {
    // 检查文件是否存在
    if (access(filePath.c_str(), F_OK) == -1) {
        return false;
    }

    // 打开并读取文件内容
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    try {
        // 解析JSON文件
        data = json::parse(file);
        return true;
    } catch (const json::parse_error& e) {
        return false;
    }
}

json readSiteConfig(const std::string& filePath) {
    json config;
    if (parseJsonFile(filePath, config)) {
        std::cout << "成功读取配置文件: " << filePath << std::endl;
        return config;
    } else {
        std::cerr << "无法读取配置文件: " << filePath << std::endl;
        return json();
    }
}

// Trim from the start (in place)
inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
}

// Trim from the end (in place)
inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

// Trim from both ends (in place)
std::string trim(const std::string &s) {
    std::string tmp = s;
    rtrim(tmp);
    ltrim(tmp);
    return tmp;
}

void showVideoList(const std::map<std::string, std::vector<VideoInfo>>& videoList) {
    for (const auto& [name, videos] : videoList) {
        std::cout << "\n名称: " << name << std::endl;
        std::cout << "数量: " << videos.size() << std::endl;
        for (const auto& video : videos) {
            std::cout << "|-ID: " << video.vod_id << std::endl;
            std::cout << "|-源: " << video.source << std::endl;
            for (auto &[source, urls] : video.play_urls) {
                std::cout << "| |-播放源: " << source << std::endl;
                for (auto &[name, url] : urls) {
                    std::cout << "| | |-" << trim(name) << " : ";
                    std::cout << url << std::endl;
                }
            }
        }
    }
}

int main() {
    WebServer webServer;

    auto videoList = webServer.getVideoList();
    // showVideoList(videoList);

    webServer.setVideoList(videoList);

    // 在后台线程中启动Web服务器
    std::thread serverThread([&webServer]() {
        webServer.run(8080);
    });

    // 分离线程，让Web服务器在后台运行
    serverThread.detach();

    // 等待用户输入以退出程序
    std::cout << "Web服务器已启动，按回车键退出..." << std::endl;
    std::cin.get();

    return 0;
}
