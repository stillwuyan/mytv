#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// 视频信息结构体
struct VideoInfo {
    int vod_id = 0;
    std::string source;
    std::string vod_name;
    std::string vod_sub;
    std::string vod_remarks;
    std::string vod_pic;
    std::string vod_content;
    std::map<std::string, std::vector<std::pair<std::string, std::string>>> play_urls;
};

struct VideoParseResult {
    std::vector<VideoInfo> videos;
    std::size_t skippedCount = 0;
};

class JsonParser {
public:
    JsonParser();

    // 从文件解析JSON
    bool parseFromFile(const std::string& filePath);

    // 获取视频列表
    std::vector<VideoInfo> getVideoList() const;

    // 获取视频列表及跳过统计
    VideoParseResult getVideoListWithStats() const;

private:
    // 解析单个视频信息
    VideoInfo parseVideoInfo(const json& videoJson) const;

    // 解析播放URL
    std::vector<std::pair<std::string, std::string>>
    parsePlayUrls(const std::string& playUrlString) const;

    // 按分隔符拆分字符串
    std::vector<std::string>
    splitString(const std::string& str, const std::string& delimiter) const;

    json data_;
    std::string source_;
    bool isParsed_;

};

#endif // JSON_PARSER_H
