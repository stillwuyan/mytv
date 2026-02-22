#include <unistd.h>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>
#include "web_server.h"
#include "https_json_client.h"

using json = nlohmann::json;

const std::string WebServer::INPUT_PATH = "../input/";
const std::string WebServer::OUTPUT_PATH = "../output/";

WebServer::WebServer() {
    // 初始化Crow应用
}

WebServer::~WebServer() {
    // 清理资源
}

void WebServer::run(int port) {
    setupRoutes();
    std::cout << "Web服务器启动在端口: " << port << std::endl;
    std::cout << "访问 http://localhost:" << port << " 查看视频列表" << std::endl;
    app.port(port).multithreaded().run();
}

void WebServer::setVideoList(const std::map<std::string, std::vector<VideoInfo>>& data) {
    videoList = data;
}

std::map<std::string, std::vector<VideoInfo>> WebServer::getVideoList() {
    std::map<std::string, std::vector<VideoInfo>> allVideos;
    JsonParser parser;

    try {
        // 遍历output目录下的所有文件
        for (const auto& entry : std::filesystem::directory_iterator(OUTPUT_PATH)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                std::string filePath = entry.path().string();

                // 解析JSON文件
                if (parser.parseFromFile(filePath)) {
                    // 获取video list并添加到结果中
                    std::vector<VideoInfo> videos = parser.getVideoList();
                    for (const auto& video : videos) {
                        allVideos[video.vod_name].push_back(video);
                    }
                    std::cout << "成功解析文件: " << filePath << ", 找到 " << videos.size() << " 个视频" << std::endl;
                } else {
                    std::cerr << "解析失败: " << filePath << std::endl;
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "文件系统错误: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "解析过程中发生错误: " << e.what() << std::endl;
    }

    return allVideos;
}

// 在 setupRoutes 方法中添加搜索端点
void WebServer::setupRoutes() {
    // 静态文件服务 - 提供前端页面
    CROW_ROUTE(app, "/front/<path>")
    ([](const crow::request& req, crow::response& res, std::string path) {
        std::string file_path = "../front/" + path;

        // 检查文件是否存在
        if (!std::filesystem::exists(file_path)) {
            res.code = 404;
            res.end("File not found");
            return;
        }

        // 设置MIME类型
        // 替换 string::ends_with 为 C++17 兼容的代码
        if (path.size() >= 5 && path.compare(path.size() - 5, 5, ".html") == 0) {
            res.set_header("Content-Type", "text/html");
        } else if (path.size() >= 4 && path.compare(path.size() - 4, 4, ".css") == 0) {
            res.set_header("Content-Type", "text/css");
        } else if (path.size() >= 3 && path.compare(path.size() - 3, 3, ".js") == 0) {
            res.set_header("Content-Type", "application/javascript");
        }

        // 读取并返回文件内容
        std::ifstream file(file_path, std::ios::binary);
        if (file) {
            // 将文件内容读取到字符串
            std::string content((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
            res.write(content);
            res.end();
        } else {
            res.code = 500;
            res.end("Error reading file");
        }
    });

    // 首页路由 - 重定向到前端页面
    CROW_ROUTE(app, "/")
    ([]() {
        crow::response res(302);
        res.set_header("Location", "/front/index.html");
        return res;
    });

    // JSON API路由 - 返回视频数据的JSON格式
    CROW_ROUTE(app, "/api/videos")
    ([this]() {
        crow::json::wvalue result;
        int index = 0;

        for (const auto& [name, videos] : videoList) {
            crow::json::wvalue videoArray;
            int videoIndex = 0;

            for (const auto& video : videos) {
                crow::json::wvalue videoObj;
                videoObj["vod_id"] = video.vod_id;
                videoObj["vod_name"] = video.vod_name;
                videoObj["source"] = video.source;
                videoObj["vod_sub"] = video.vod_sub;          // vod_sub字段
                videoObj["vod_content"] = video.vod_content;  // vod_content字段

                crow::json::wvalue playUrls;
                for (const auto& [source, urls] : video.play_urls) {
                    crow::json::wvalue urlArray;
                    int urlIndex = 0;

                    for (const auto& [epName, url] : urls) {
                        crow::json::wvalue urlObj;
                        urlObj["name"] = epName;
                        urlObj["url"] = url;
                        urlArray[urlIndex++] = std::move(urlObj);
                    }

                    playUrls[source] = std::move(urlArray);
                }

                videoObj["play_urls"] = std::move(playUrls);
                videoArray[videoIndex++] = std::move(videoObj);
            }

            result[name] = std::move(videoArray);
            index++;
        }

        return crow::response(result);
    });

    // 添加搜索端点
    CROW_ROUTE(app, "/api/search")
    .methods("POST"_method)
    ([this](const crow::request& req) {
        auto x = crow::json::load(req.body);
        if (!x || !x.has("keyword")) {
            return crow::response(400, "Missing keyword parameter");
        }

        bool result = deleteOutputJsonFiles();
        if (!result) {
            return crow::response(500, "Failed to delete input JSON files");
        }

        std::string keyword = x["keyword"].s();
        result = search(keyword);

        if (!result) {
            return crow::response(500, "Search failed");
        }

        // 重新加载视频数据
        videoList = getVideoList();
        return crow::response(200, "Search completed successfully");
    });
}

bool WebServer::search(const std::string& key) {
    try {
        // 创建HTTP客户端
        HTTPSJsonClient client;
        client.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122.0.0.0 Safari/537.36");
        client.setConnectTimeout(10);
        client.setRequestTimeout(30);
        client.setVerifySSL(false);

        const std::string sourceFile = INPUT_PATH + "source.json";

        // 将 readSiteConfig 函数移动到 WebServer 类中
        json source = readSiteConfig(sourceFile);
        if (source.empty()) {
            std::cerr << "无法读取配置文件: " << sourceFile << std::endl;
            return false;
        }

        const std::string encodeKey = client.urlEncode(key);
        const std::string ext = ".json";
        json siteList = source["api_site"];

        for (const auto& [domain, site] : siteList.items()) {
            std::cout << "\n查询 " <<  site["name"] << std::endl;

            // 发送GET请求
            std::string url = site["api"].get<std::string>() + "?ac=videolist&wd=" + encodeKey;
            std::string response = client.get(url);

            if (!response.empty() && client.getLastStatusCode() == 200) {
                std::cout << "请求成功！" << std::endl;

                // 将响应内容保存到文件
                std::string filename = domain;
                std::replace(filename.begin(), filename.end(), '.', '_');

                std::ofstream outFile(OUTPUT_PATH + filename + ext);
                if (outFile.is_open()) {
                    outFile << response;
                    outFile.close();
                    std::cout << "响应已保存到文件: " << filename << std::endl;
                } else {
                    std::cerr << "无法创建文件: " << filename << std::endl;
                }
            } else {
                std::cerr << "请求失败: " << client.getLastError() << std::endl;
                std::cerr << "HTTP状态码: " << client.getLastStatusCode() << std::endl;
                std::cerr << "请求命令：" << url << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "程序异常: " << e.what() << std::endl;
        return false;
    }

    return true;
}

// 实现 readSiteConfig 方法
json WebServer::readSiteConfig(const std::string& filePath) {
    json config;

    // 检查文件是否存在
    if (access(filePath.c_str(), F_OK) == -1) {
        std::cerr << "配置文件不存在: " << filePath << std::endl;
        return json();
    }

    // 打开并读取文件内容
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "无法打开配置文件: " << filePath << std::endl;
        return json();
    }

    try {
        // 解析JSON文件
        config = json::parse(file);
        std::cout << "成功读取配置文件: " << filePath << std::endl;
        return config;
    } catch (const json::parse_error& e) {
        std::cerr << "JSON解析错误: " << e.what() << std::endl;
        return json();
    }
}

// OUTPUT_PATH路径下所有JSON文件的方法
bool WebServer::deleteOutputJsonFiles() {
    try {
        std::filesystem::path outputPath(OUTPUT_PATH);

        // 检查目录是否存在
        if (!std::filesystem::exists(outputPath)) {
            std::cerr << "输入目录不存在: " << OUTPUT_PATH << std::endl;
            return false;
        }

        int deletedCount = 0;

        // 遍历目录并删除所有.json文件
        for (const auto& entry : std::filesystem::directory_iterator(outputPath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                if (std::filesystem::remove(entry.path())) {
                    std::cout << "已删除文件: " << entry.path().filename() << std::endl;
                    deletedCount++;
                } else {
                    std::cerr << "删除文件失败: " << entry.path().filename() << std::endl;
                }
            }
        }

        std::cout << "共删除 " << deletedCount << " 个JSON文件" << std::endl;
        return true;

    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "文件系统错误: " << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "删除文件时发生错误: " << e.what() << std::endl;
        return false;
    }
}
