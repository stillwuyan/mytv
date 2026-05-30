#include <fstream>
#include <algorithm>
#include <deque>
#include <future>
#include <filesystem>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <chrono>
#include <nlohmann/json.hpp>
#include "web_server.h"
#include "https_json_client.h"

using json = nlohmann::json;

namespace {
struct SearchStats {
    int attemptedSites = 0;
    int skippedSites = 0;
    int successfulResponses = 0;
    int savedFiles = 0;
};

struct SiteSearchResult {
    std::string domain;
    std::string siteName;
    bool requestSucceeded = false;
    bool fileSaved = false;
};

struct CatalogLoadStats {
    int parsedFiles = 0;
    int skippedFiles = 0;
    int loadedVideos = 0;
    int skippedVideos = 0;
};

constexpr std::size_t kMaxConcurrentSearches = 4;
constexpr int kMaxSiteFailureCount = 5;
constexpr const char* kLogModule = "WebServer";
constexpr const char* kSiteUpdateUrl = "https://pz.v88.qzz.io/?format=0&source=jin18";

enum class LogLevel {
    Info,
    Error
};

std::mutex logMutex;

template <typename... Args>
void logMessage(LogLevel level, Args&&... args) {
    std::ostringstream buffer;
    (buffer << ... << std::forward<Args>(args));

    const auto now = std::chrono::system_clock::now();
    const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
    std::tm timeInfo{};
#ifdef _WIN32
    localtime_s(&timeInfo, &nowTime);
#else
    localtime_r(&nowTime, &timeInfo);
#endif

    std::ostringstream timestamp;
    timestamp << std::put_time(&timeInfo, "%Y-%m-%d %H:%M:%S");

    std::lock_guard<std::mutex> lock(logMutex);
    std::ostream& stream = level == LogLevel::Error ? std::cerr : std::cout;
    stream << '[' << timestamp.str() << "] [" << kLogModule << "] "
           << (level == LogLevel::Error ? "[ERROR] " : "[INFO] ")
           << buffer.str() << std::endl;
}

template <typename... Args>
void logInfo(Args&&... args) {
    logMessage(LogLevel::Info, std::forward<Args>(args)...);
}

template <typename... Args>
void logError(Args&&... args) {
    logMessage(LogLevel::Error, std::forward<Args>(args)...);
}

bool isSubPath(const std::filesystem::path& base, const std::filesystem::path& candidate) {
    auto baseIt = base.begin();
    auto candidateIt = candidate.begin();

    for (; baseIt != base.end() && candidateIt != candidate.end(); ++baseIt, ++candidateIt) {
        if (*baseIt != *candidateIt) {
            return false;
        }
    }

    return baseIt == base.end();
}

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }

    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

const char* contentTypeForPath(const std::string& path) {
    if (path.size() >= 5 && path.compare(path.size() - 5, 5, ".html") == 0) {
        return "text/html";
    }
    if (path.size() >= 4 && path.compare(path.size() - 4, 4, ".css") == 0) {
        return "text/css";
    }
    if (path.size() >= 3 && path.compare(path.size() - 3, 3, ".js") == 0) {
        return "application/javascript";
    }
    return nullptr;
}

bool readFileContent(const std::filesystem::path& filePath, std::string& content) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        return false;
    }

    content.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return true;
}

bool writeFileContent(const std::filesystem::path& filePath, const std::string& content) {
    std::ofstream file(filePath, std::ios::binary | std::ios::trunc);
    if (!file) {
        return false;
    }

    file.write(content.data(), static_cast<std::streamsize>(content.size()));
    return static_cast<bool>(file);
}

crow::response makeJsonResponse(int code, bool ok, const std::string& message) {
    crow::json::wvalue body;
    body["ok"] = ok;
    body["message"] = message;
    return crow::response(code, body);
}

crow::json::wvalue toPlayUrlsJson(const VideoInfo& video) {
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

    return playUrls;
}

crow::json::wvalue toVideoJson(const VideoInfo& video) {
    crow::json::wvalue videoObj;
    videoObj["vod_id"] = video.vod_id;
    videoObj["vod_name"] = video.vod_name;
    videoObj["source"] = video.source;
    videoObj["vod_sub"] = video.vod_sub;
    videoObj["vod_content"] = video.vod_content;
    videoObj["play_urls"] = toPlayUrlsJson(video);
    return videoObj;
}

crow::json::wvalue toCatalogJson(const std::map<std::string, std::vector<VideoInfo>>& catalog) {
    crow::json::wvalue result;

    for (const auto& [name, videos] : catalog) {
        crow::json::wvalue videoArray;
        int videoIndex = 0;

        for (const auto& video : videos) {
            videoArray[videoIndex++] = toVideoJson(video);
        }

        result[name] = std::move(videoArray);
    }

    return result;
}

bool ensureDirectoryExists(const std::filesystem::path& dirPath, const std::string& description) {
    std::error_code ec;
    if (std::filesystem::exists(dirPath, ec)) {
        if (ec) {
            logError(description, "检查失败: ", ec.message());
            return false;
        }
        if (!std::filesystem::is_directory(dirPath)) {
            logError(description, "不是目录: ", dirPath);
            return false;
        }
        return true;
    }

    std::filesystem::create_directories(dirPath, ec);
    if (ec) {
        logError("无法创建", description, ": ", dirPath, ", 错误: ", ec.message());
        return false;
    }

    logInfo("已创建", description, ": ", dirPath);
    return true;
}

bool backupFileReplacingPrevious(const std::filesystem::path& sourceFile, const std::filesystem::path& backupFile) {
    std::error_code ec;
    if (std::filesystem::exists(backupFile, ec)) {
        if (ec) {
            return false;
        }
        std::filesystem::remove(backupFile, ec);
        if (ec) {
            return false;
        }
    }

    std::filesystem::copy_file(sourceFile, backupFile, std::filesystem::copy_options::overwrite_existing, ec);
    return !ec;
}

bool saveSearchResult(const std::filesystem::path& outputDir, const std::string& domain, const std::string& response) {
    std::string filename = domain;
    std::replace(filename.begin(), filename.end(), '.', '_');

    std::ofstream outFile(outputDir / (filename + ".json"));
    if (!outFile.is_open()) {
        logError("无法创建文件: ", filename);
        return false;
    }

    outFile << response;
    logInfo("响应已保存到文件: ", filename);
    return true;
}

json loadApiSites(const json& source, const std::string& sourceFile) {
    if (!source.contains("api_site") || !source["api_site"].is_object()) {
        logError("配置文件缺少 api_site 对象: ", sourceFile);
        return json();
    }

    return source["api_site"];
}

void configureSearchClient(HTTPSJsonClient& client) {
    client.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122.0.0.0 Safari/537.36");
    client.setConnectTimeout(5);
    client.setRequestTimeout(10);
    client.setVerifySSL(true);
}

SiteSearchResult searchSingleSite(
    const std::filesystem::path& outputDir,
    const std::string& domain,
    const json& site,
    const std::string& encodedKeyword) {
    SiteSearchResult result;
    result.domain = domain;

    try {
        HTTPSJsonClient client;
        configureSearchClient(client);

        if (!site.contains("api") || !site["api"].is_string()) {
            logError("站点配置缺少 api 字段: ", domain);
            return result;
        }

        const std::string siteName = site.value("name", domain);
        result.siteName = siteName;
        logInfo("查询 ", siteName);

        const std::string url = site["api"].get<std::string>() + "?ac=videolist&wd=" + encodedKeyword;
        const auto start = std::chrono::steady_clock::now();
        const std::string response = client.get(url);
        const auto end = std::chrono::steady_clock::now();
        logInfo("站点 ", siteName, " 请求耗时: ", std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count(), "ms");

        if (response.empty() || client.getLastStatusCode() != 200) {
            logError("站点请求失败: ", siteName, ", error=", client.getLastError(), ", status=", client.getLastStatusCode(), ", url=", url);
            return result;
        }

        result.requestSucceeded = true;
        logInfo("站点请求成功: ", siteName);
        result.fileSaved = saveSearchResult(outputDir, domain, response);
        return result;
    } catch (const std::exception& e) {
        if (result.siteName.empty()) {
            result.siteName = domain;
        }
        logError("站点搜索异常: ", domain, ", error=", e.what());
        return result;
    }
}

std::vector<std::filesystem::path> collectJsonFiles(const std::filesystem::path& outputPath) {
    std::vector<std::filesystem::path> jsonFiles;

    for (const auto& entry : std::filesystem::directory_iterator(outputPath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            jsonFiles.push_back(entry.path());
        }
    }

    return jsonFiles;
}

std::string normalizeSiteFileKey(std::string value) {
    std::replace(value.begin(), value.end(), '.', '_');
    return value;
}

std::map<std::string, std::string> buildSiteDisplayNames(const json& siteList) {
    std::map<std::string, std::string> siteDisplayNames;

    if (!siteList.is_object()) {
        return siteDisplayNames;
    }

    for (const auto& [domain, site] : siteList.items()) {
        siteDisplayNames[normalizeSiteFileKey(domain)] = site.value("name", domain);
    }

    return siteDisplayNames;
}

bool loadVideosFromJsonFile(
    JsonParser& parser,
    const std::filesystem::path& filePath,
    std::map<std::string, std::vector<VideoInfo>>& allVideos,
    CatalogLoadStats& stats,
    const std::map<std::string, std::string>& siteDisplayNames) {
    try {
        if (!parser.parseFromFile(filePath.string())) {
            logError("解析失败，跳过文件: ", filePath);
            stats.skippedFiles++;
            return false;
        }

        VideoParseResult parseResult = parser.getVideoListWithStats();
        for (auto& video : parseResult.videos) {
            const auto displayNameIt = siteDisplayNames.find(video.source);
            if (displayNameIt != siteDisplayNames.end()) {
                video.source = displayNameIt->second;
            }
            allVideos[video.vod_name].push_back(video);
        }

        stats.parsedFiles++;
        stats.loadedVideos += static_cast<int>(parseResult.videos.size());
        stats.skippedVideos += static_cast<int>(parseResult.skippedCount);
        logInfo("成功解析文件: ", filePath,
                ", 成功 ", parseResult.videos.size(),
                " 个视频, 跳过 ", parseResult.skippedCount, " 个条目");
        return true;
    } catch (const std::exception& e) {
        logError("解析文件异常，已跳过: ", filePath, ", error=", e.what());
        stats.skippedFiles++;
        return false;
    }
}

CatalogLoadStats collectVideoCatalog(
    const std::vector<std::filesystem::path>& jsonFiles,
    std::map<std::string, std::vector<VideoInfo>>& allVideos,
    const std::map<std::string, std::string>& siteDisplayNames) {
    CatalogLoadStats stats;
    JsonParser parser;
    for (const auto& filePath : jsonFiles) {
        loadVideosFromJsonFile(parser, filePath, allVideos, stats, siteDisplayNames);
    }

    return stats;
}

SiteSearchResult consumeCompletedSearchTask(std::deque<std::future<SiteSearchResult>>& tasks) {
    SiteSearchResult siteResult = tasks.front().get();
    tasks.pop_front();

    return siteResult;
}

bool createDirectory(const std::filesystem::path& dirPath, const std::string& errorPrefix) {
    std::error_code ec;
    std::filesystem::create_directories(dirPath, ec);
    if (ec) {
        logError(errorPrefix, dirPath, ", 错误: ", ec.message());
        return false;
    }

    return true;
}

bool createBackupDirectory(const std::filesystem::path& outputPath, std::filesystem::path& backupRoot, std::filesystem::path& backupDir) {
    backupRoot = outputPath.parent_path() / (outputPath.filename().string() + "_backup");
    if (!createDirectory(backupRoot, "无法创建备份目录: ")) {
        return false;
    }

    const auto now = std::chrono::system_clock::now();
    const auto ts = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    backupDir = backupRoot / ("backup_" + std::to_string(ts));
    return createDirectory(backupDir, "无法创建备份子目录: ");
}

int backupFiles(const std::vector<std::filesystem::path>& jsonFiles, const std::filesystem::path& backupDir) {
    int copiedCount = 0;

    for (const auto& p : jsonFiles) {
        try {
            std::filesystem::copy_file(p, backupDir / p.filename(), std::filesystem::copy_options::overwrite_existing);
            copiedCount++;
        } catch (const std::filesystem::filesystem_error& e) {
            logError("备份文件失败: ", p.filename(), ", 错误: ", e.what());
        }
    }

    return copiedCount;
}

void pruneOldBackups(const std::filesystem::path& backupRoot) {
    std::vector<std::filesystem::path> backups;
    for (const auto& entry : std::filesystem::directory_iterator(backupRoot)) {
        if (entry.is_directory()) {
            backups.push_back(entry.path());
        }
    }

    if (backups.size() <= 1) {
        return;
    }

    std::sort(backups.begin(), backups.end(), [](const std::filesystem::path& a, const std::filesystem::path& b) {
        std::error_code e1, e2;
        const auto t1 = std::filesystem::last_write_time(a, e1);
        const auto t2 = std::filesystem::last_write_time(b, e2);
        if (e1 || e2) return a.string() < b.string();
        return t1 < t2;
    });

    for (size_t i = 0; i + 1 < backups.size(); ++i) {
        try {
            std::filesystem::remove_all(backups[i]);
            logInfo("已删除旧备份: ", backups[i].filename());
        } catch (const std::filesystem::filesystem_error& e) {
            logError("删除旧备份失败: ", backups[i].filename(), ", 错误: ", e.what());
        }
    }
}

int deleteFiles(const std::vector<std::filesystem::path>& jsonFiles) {
    int deletedCount = 0;

    for (const auto& p : jsonFiles) {
        try {
            if (std::filesystem::remove(p)) {
                logInfo("已删除文件: ", p.filename());
                deletedCount++;
            } else {
                logError("删除文件失败: ", p.filename());
            }
        } catch (const std::filesystem::filesystem_error& e) {
            logError("删除文件异常: ", p.filename(), ", 错误: ", e.what());
        }
    }

    return deletedCount;
}

crow::response serveFrontFile(const std::string& path) {
    const std::filesystem::path baseDir = "../front";
    if (!std::filesystem::exists(baseDir) || !std::filesystem::is_directory(baseDir)) {
        return crow::response(500, "Front directory not found");
    }

    const std::filesystem::path requested = baseDir / path;

    try {
        if (!std::filesystem::exists(requested) || !std::filesystem::is_regular_file(requested)) {
            return crow::response(404, "File not found");
        }

        const auto canonicalBase = std::filesystem::canonical(baseDir);
        const auto canonicalRequested = std::filesystem::canonical(requested);
        if (!isSubPath(canonicalBase, canonicalRequested)) {
            return crow::response(403, "Forbidden");
        }

        std::string content;
        if (!readFileContent(canonicalRequested, content)) {
            return crow::response(500, "Error reading file");
        }

        crow::response res;
        if (const char* contentType = contentTypeForPath(path)) {
            res.set_header("Content-Type", contentType);
        }
        res.write(content);
        return res;
    } catch (const std::filesystem::filesystem_error&) {
        return crow::response(404, "File not found");
    }
}
}

const std::string WebServer::INPUT_PATH = "../input/";
const std::string WebServer::OUTPUT_PATH = "../output/";

bool WebServer::shouldSkipSite(const std::string& domain, int maxFailures) const {
    std::lock_guard<std::mutex> lock(siteFailureCountsMutex);
    const auto it = siteFailureCounts.find(domain);
    return it != siteFailureCounts.end() && it->second >= maxFailures;
}

int WebServer::recordSiteRequestResult(const std::string& domain, bool requestSucceeded) {
    std::lock_guard<std::mutex> lock(siteFailureCountsMutex);

    if (requestSucceeded) {
        siteFailureCounts.erase(domain);
        return 0;
    }

    int& failureCount = siteFailureCounts[domain];
    failureCount++;
    return failureCount;
}

void WebServer::run(int port) {
    setupRoutes();
    logInfo("Web服务器启动在端口: ", port);
    logInfo("访问 http://localhost:", port, " 查看视频列表");
    app.port(port).multithreaded().run();
}

void WebServer::setVideoList(const std::map<std::string, std::vector<VideoInfo>>& data) {
    std::lock_guard<std::mutex> lock(videoListMutex);
    videoList = data;
}

std::map<std::string, std::vector<VideoInfo>> WebServer::getVideoList() {
    std::map<std::string, std::vector<VideoInfo>> allVideos;
    const std::filesystem::path outputPath(OUTPUT_PATH);

    try {
        if (!std::filesystem::exists(outputPath)) {
            logError("输出目录不存在: ", outputPath);
            return allVideos;
        }

        const std::vector<std::filesystem::path> jsonFiles = collectJsonFiles(outputPath);
        std::map<std::string, std::string> siteDisplayNames;
        const std::string sourceFile = INPUT_PATH + "source.json";
        const json sourceConfig = readSiteConfig(sourceFile);
        if (!sourceConfig.empty()) {
            siteDisplayNames = buildSiteDisplayNames(loadApiSites(sourceConfig, sourceFile));
        }

        const CatalogLoadStats stats = collectVideoCatalog(jsonFiles, allVideos, siteDisplayNames);
        logInfo("目录解析完成: 文件总数=", jsonFiles.size(),
                ", 成功文件=", stats.parsedFiles,
                ", 跳过文件=", stats.skippedFiles,
                ", 成功视频=", stats.loadedVideos,
                ", 跳过条目=", stats.skippedVideos);
    } catch (const std::filesystem::filesystem_error& e) {
        logError("文件系统错误: ", e.what());
    } catch (const std::exception& e) {
        logError("解析过程中发生错误: ", e.what());
    }

    return allVideos;
}

// 在 setupRoutes 方法中添加搜索端点
void WebServer::setupRoutes() {
    // 静态文件服务 - 提供前端页面
    CROW_ROUTE(app, "/front/<path>")
    ([](const crow::request&, std::string path) {
        return serveFrontFile(path);
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
        std::map<std::string, std::vector<VideoInfo>> snapshot;
        {
            std::lock_guard<std::mutex> lock(videoListMutex);
            snapshot = videoList;
        }

        return crow::response(toCatalogJson(snapshot));
    });

    // 添加搜索端点
    CROW_ROUTE(app, "/api/search")
    .methods("POST"_method)
    ([this](const crow::request& req) {
        const auto x = crow::json::load(req.body);
        if (!x || !x.has("keyword")) {
            return makeJsonResponse(400, false, "Missing keyword parameter");
        }

        const std::string keyword = trim(x["keyword"].s());
        if (keyword.empty()) {
            return makeJsonResponse(400, false, "Keyword cannot be empty");
        }

        bool result = deleteOutputJsonFiles();
        if (!result) {
            return makeJsonResponse(500, false, "Failed to reset cached search results");
        }

        result = search(keyword);

        if (!result) {
            return makeJsonResponse(500, false, "Search failed or returned no valid sources");
        }

        setVideoList(getVideoList());
        return makeJsonResponse(200, true, "Search completed successfully");
    });

    CROW_ROUTE(app, "/api/update")
    .methods("POST"_method)
    ([this]() {
        if (!updateSiteConfig()) {
            return makeJsonResponse(500, false, "Failed to update source.json");
        }

        return makeJsonResponse(200, true, "站点配置已更新");
    });
}

bool WebServer::search(const std::string& key) {
    try {
        const std::string sourceFile = INPUT_PATH + "source.json";
        json source = readSiteConfig(sourceFile);
        if (source.empty()) {
            logError("无法读取配置文件: ", sourceFile);
            return false;
        }

        const std::filesystem::path outputDir(OUTPUT_PATH);
        if (!ensureDirectoryExists(outputDir, "输出目录")) {
            return false;
        }

        HTTPSJsonClient encodeClient;
        const std::string encodeKey = encodeClient.urlEncode(key);
        const json siteList = loadApiSites(source, sourceFile);
        if (siteList.empty()) {
            return false;
        }

        SearchStats stats;
        std::deque<std::future<SiteSearchResult>> tasks;

        for (const auto& [domain, site] : siteList.items()) {
            const std::string siteName = site.value("name", domain);

            if (shouldSkipSite(domain, kMaxSiteFailureCount)) {
                stats.skippedSites++;
                logInfo("站点已累计失败 ", kMaxSiteFailureCount, " 次，跳过本次搜索: ", siteName);
                continue;
            }

            stats.attemptedSites++;

            if (tasks.size() >= kMaxConcurrentSearches) {
                SiteSearchResult siteResult = consumeCompletedSearchTask(tasks);
                if (siteResult.requestSucceeded) {
                    stats.successfulResponses++;
                }
                if (siteResult.fileSaved) {
                    stats.savedFiles++;
                }

                const int failureCount = recordSiteRequestResult(siteResult.domain, siteResult.requestSucceeded);
                if (!siteResult.requestSucceeded) {
                    logError("站点失败计数更新: ", siteResult.siteName.empty() ? siteResult.domain : siteResult.siteName,
                             ", failures=", failureCount, "/", kMaxSiteFailureCount);
                }
            }

            tasks.push_back(std::async(std::launch::async, [outputDir, domain, site, encodeKey]() {
                return searchSingleSite(outputDir, domain, site, encodeKey);
            }));
        }

        while (!tasks.empty()) {
            SiteSearchResult siteResult = consumeCompletedSearchTask(tasks);
            if (siteResult.requestSucceeded) {
                stats.successfulResponses++;
            }
            if (siteResult.fileSaved) {
                stats.savedFiles++;
            }

            const int failureCount = recordSiteRequestResult(siteResult.domain, siteResult.requestSucceeded);
            if (!siteResult.requestSucceeded) {
                logError("站点失败计数更新: ", siteResult.siteName.empty() ? siteResult.domain : siteResult.siteName,
                         ", failures=", failureCount, "/", kMaxSiteFailureCount);
            }
        }

        logInfo("搜索完成: 共尝试 ", stats.attemptedSites,
                " 个站点, 跳过 ", stats.skippedSites,
                " 个站点, 成功响应 ", stats.successfulResponses,
                " 个, 落盘 ", stats.savedFiles, " 个文件");

        if (stats.savedFiles == 0) {
            logError("没有任何站点返回可保存的搜索结果");
            return false;
        }
    } catch (const std::exception& e) {
        logError("程序异常: ", e.what());
        return false;
    }

    return true;
}

bool WebServer::updateSiteConfig() {
    try {
        const std::filesystem::path inputDir(INPUT_PATH);
        if (!ensureDirectoryExists(inputDir, "输入目录")) {
            return false;
        }

        const std::filesystem::path sourceFile = inputDir / "source.json";
        const std::filesystem::path backupFile = inputDir / "source.backup.json";

        HTTPSJsonClient client;
        configureSearchClient(client);
        client.setRequestTimeout(15);

        logInfo("开始更新站点配置: ", kSiteUpdateUrl);
        const std::string response = client.get(kSiteUpdateUrl);
        if (response.empty() || client.getLastStatusCode() != 200) {
            logError("下载站点配置失败: error=", client.getLastError(), ", status=", client.getLastStatusCode(), ", url=", kSiteUpdateUrl);
            return false;
        }

        json parsed;
        try {
            parsed = json::parse(response);
        } catch (const json::parse_error& e) {
            logError("下载的站点配置不是有效JSON: ", e.what());
            return false;
        }

        if (!parsed.contains("api_site") || !parsed["api_site"].is_object()) {
            logError("下载的站点配置缺少 api_site 对象");
            return false;
        }

        const std::string formattedJson = parsed.dump(4);

        if (std::filesystem::exists(sourceFile)) {
            if (!backupFileReplacingPrevious(sourceFile, backupFile)) {
                logError("备份 source.json 失败: ", backupFile);
                return false;
            }
            logInfo("已备份站点配置到: ", backupFile);
        }

        if (!writeFileContent(sourceFile, formattedJson)) {
            logError("写入站点配置失败: ", sourceFile);
            return false;
        }

        logInfo("站点配置更新完成: ", sourceFile);
        return true;
    } catch (const std::filesystem::filesystem_error& e) {
        logError("更新站点配置时发生文件系统错误: ", e.what());
        return false;
    } catch (const std::exception& e) {
        logError("更新站点配置时发生异常: ", e.what());
        return false;
    }
}

// 实现 readSiteConfig 方法
json WebServer::readSiteConfig(const std::string& filePath) {
    json config;

    if (!std::filesystem::exists(filePath)) {
        logError("配置文件不存在: ", filePath);
        return json();
    }

    // 打开并读取文件内容
    std::ifstream file(filePath);
    if (!file.is_open()) {
        logError("无法打开配置文件: ", filePath);
        return json();
    }

    try {
        // 解析JSON文件
        config = json::parse(file);
        logInfo("成功读取配置文件: ", filePath);
        return config;
    } catch (const json::parse_error& e) {
        logError("JSON解析错误: ", e.what());
        return json();
    }
}

// OUTPUT_PATH路径下所有JSON文件的方法
bool WebServer::deleteOutputJsonFiles() {
    try {
        const std::filesystem::path outputPath(OUTPUT_PATH);

        if (!std::filesystem::exists(outputPath)) {
            return ensureDirectoryExists(outputPath, "输出目录");
        }

        const std::vector<std::filesystem::path> jsonFiles = collectJsonFiles(outputPath);

        if (jsonFiles.empty()) {
            logInfo("没有找到要删除的 JSON 文件");
            return true;
        }

        std::filesystem::path backupRoot;
        std::filesystem::path backupDir;
        if (!createBackupDirectory(outputPath, backupRoot, backupDir)) {
            return false;
        }

        const int copiedCount = backupFiles(jsonFiles, backupDir);
        pruneOldBackups(backupRoot);
        const int deletedCount = deleteFiles(jsonFiles);

        logInfo("共备份 ", copiedCount, " 个文件，已删除 ", deletedCount, " 个JSON文件");
        logInfo("最新备份位置: ", backupDir);
        return true;

    } catch (const std::filesystem::filesystem_error& e) {
        logError("文件系统错误: ", e.what());
        return false;
    } catch (const std::exception& e) {
        logError("删除文件时发生错误: ", e.what());
        return false;
    }
}
