#include "json_parser.h"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <utility>

namespace {
constexpr const char* kLogModule = "JsonParser";

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
}

JsonParser::JsonParser() : isParsed_(false) {}

bool JsonParser::parseFromFile(const std::string& filePath) {
    isParsed_ = false;
    data_ = json();

    std::ifstream file(filePath);
    if (!file.is_open()) {
        logError("无法打开文件: ", filePath);
        return false;
    }

    try {
        data_ = json::parse(file);
        source_ = std::filesystem::path(filePath).filename().stem().string();
        isParsed_ = true;
        return true;
    } catch (const json::parse_error& e) {
        logError("JSON解析错误: file=", filePath, ", error=", e.what());
        return false;
    }
}

std::vector<VideoInfo> JsonParser::getVideoList() const {
    return getVideoListWithStats().videos;
}

VideoParseResult JsonParser::getVideoListWithStats() const {
    VideoParseResult result;

    if (!isParsed_ || !data_.contains("list") || !data_["list"].is_array()) {
        return result;
    }

    for (const auto& videoJson : data_["list"]) {
        try {
            result.videos.push_back(parseVideoInfo(videoJson));
        } catch (const json::exception& e) {
            result.skippedCount++;
            logError("视频条目解析失败: source=", source_, ", error=", e.what());
        } catch (const std::exception& e) {
            result.skippedCount++;
            logError("视频条目解析异常: source=", source_, ", error=", e.what());
        }
    }

    return result;
}

VideoInfo JsonParser::parseVideoInfo(const json& videoJson) const {
    VideoInfo info;
    info.source = source_;

    if (videoJson.contains("vod_id")) info.vod_id = videoJson["vod_id"].get<int>();
    if (videoJson.contains("vod_name")) info.vod_name = videoJson["vod_name"].get<std::string>();
    if (videoJson.contains("vod_sub")) info.vod_sub = videoJson["vod_sub"].get<std::string>();
    if (videoJson.contains("vod_remarks")) info.vod_remarks = videoJson["vod_remarks"].get<std::string>();
    if (videoJson.contains("vod_pic")) info.vod_pic = videoJson["vod_pic"].get<std::string>();
    if (videoJson.contains("vod_content")) info.vod_content = videoJson["vod_content"].get<std::string>();

    if (videoJson.contains("vod_play_from") && videoJson.contains("vod_play_url")) {
        std::string playFrom = videoJson["vod_play_from"].get<std::string>();
        std::string playUrl = videoJson["vod_play_url"].get<std::string>();

        // 按"$$$"分隔符拆分vod_play_from和vod_play_url
        std::vector<std::string> playFromList = splitString(playFrom, "$$$");
        std::vector<std::string> playUrlList = splitString(playUrl, "$$$");

        if (playFromList.size() != playUrlList.size()) {
            logInfo("播放源与播放地址数量不匹配: name=", info.vod_name,
                    ", from=", playFromList.size(), ", url=", playUrlList.size());
        }

        size_t size = std::min(playFromList.size(), playUrlList.size());

        for (size_t i = 0; i < size; i++) {
            std::string fromKey = playFromList[i];
            std::string urlString = playUrlList[i];

            // 解析每个播放源的URL列表
            std::vector<std::pair<std::string, std::string>> urls = parsePlayUrls(urlString);
            info.play_urls[fromKey] = urls;
        }
    }

    return info;
}

std::vector<std::pair<std::string, std::string>>
JsonParser::parsePlayUrls(const std::string& playUrlString) const {
    std::vector<std::pair<std::string, std::string>> urls;

    std::istringstream iss(playUrlString);
    std::string episode;

    while (std::getline(iss, episode, '#')) {
        size_t dollarPos = episode.find('$');
        if (dollarPos != std::string::npos) {
            std::string episodeName = episode.substr(0, dollarPos);
            std::string episodeUrl = episode.substr(dollarPos + 1);
            urls.emplace_back(episodeName, episodeUrl);
        }
    }

    return urls;
}

// 按分隔符拆分字符串
std::vector<std::string> JsonParser::splitString(const std::string& str, const std::string& delimiter) const {
    std::vector<std::string> result;
    size_t start = 0;
    size_t end = str.find(delimiter);

    while (end != std::string::npos) {
        result.push_back(str.substr(start, end - start));
        start = end + delimiter.length();
        end = str.find(delimiter, start);
    }

    // 添加最后一个部分
    result.push_back(str.substr(start));
    return result;
}
