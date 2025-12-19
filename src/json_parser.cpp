#include "json_parser.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <utility>
#include <cassert>

JsonParser::JsonParser() : isParsed_(false) {}

JsonParser::~JsonParser() {}

bool JsonParser::parseFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "无法打开文件: " << filePath << std::endl;
        return false;
    }

    try {
        data_ = json::parse(file);
        source_ = std::filesystem::path(filePath).filename().stem();
        isParsed_ = true;
        return true;
    } catch (const json::parse_error& e) {
        std::cerr << "JSON解析错误: " << e.what() << std::endl;
        return false;
    }
}

bool JsonParser::parseFromString(const std::string& jsonString) {
    try {
        data_ = json::parse(jsonString);
        isParsed_ = true;
        return true;
    } catch (const json::parse_error& e) {
        std::cerr << "JSON解析错误: " << e.what() << std::endl;
        return false;
    }
}

json JsonParser::getJsonData() const {
    return data_;
}

std::vector<VideoInfo> JsonParser::getVideoList() const {
    std::vector<VideoInfo> videoList;

    if (!isParsed_ || !data_.contains("list") || !data_["list"].is_array()) {
        return videoList;
    }

    for (const auto& videoJson : data_["list"]) {
        videoList.push_back(parseVideoInfo(videoJson));
    }

    return videoList;
}

VideoInfo JsonParser::getVideoById(int vodId) const {
    VideoInfo videoInfo;

    if (!isParsed_ || !data_.contains("list") || !data_["list"].is_array()) {
        return videoInfo;
    }

    for (const auto& videoJson : data_["list"]) {
        if (videoJson.contains("vod_id") && videoJson["vod_id"].get<int>() == vodId) {
            return parseVideoInfo(videoJson);
        }
    }

    return videoInfo;
}

std::vector<VideoInfo> JsonParser::searchVideosByName(const std::string& name) const {
    std::vector<VideoInfo> results;

    if (!isParsed_ || !data_.contains("list") || !data_["list"].is_array()) {
        return results;
    }

    std::string searchName = name;
    std::transform(searchName.begin(), searchName.end(), searchName.begin(), ::tolower);

    for (const auto& videoJson : data_["list"]) {
        if (videoJson.contains("vod_name")) {
            std::string vodName = videoJson["vod_name"].get<std::string>();
            std::transform(vodName.begin(), vodName.end(), vodName.begin(), ::tolower);

            if (vodName.find(searchName) != std::string::npos) {
                results.push_back(parseVideoInfo(videoJson));
            }
        }
    }

    return results;
}

std::map<std::string, int> JsonParser::getCategoryStatistics() const {
    std::map<std::string, int> statistics;

    if (!isParsed_ || !data_.contains("list") || !data_["list"].is_array()) {
        return statistics;
    }

    for (const auto& videoJson : data_["list"]) {
        if (videoJson.contains("type_name")) {
            std::string category = videoJson["type_name"].get<std::string>();
            statistics[category]++;
        }
    }

    return statistics;
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

        // 确保两个列表长度相同
        assert(playFromList.size() == playUrlList.size() && "vod_play_from和vod_play_url的长度不匹配");
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
