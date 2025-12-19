#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// 视频信息结构体
struct VideoInfo {
    int vod_id;
    std::string source;
    std::string vod_name;
    std::string vod_sub;
    std::string vod_remarks;
    std::string vod_pic;
    std::string vod_content;
    std::map<std::string, std::vector<std::pair<std::string, std::string>>> play_urls;
};

class JsonParser {
public:
    JsonParser();
    ~JsonParser();

    // 从文件解析JSON
    bool parseFromFile(const std::string& filePath);

    // 从字符串解析JSON
    bool parseFromString(const std::string& jsonString);

    // 获取解析后的JSON对象
    json getJsonData() const;

    // 获取视频列表
    std::vector<VideoInfo> getVideoList() const;

    // 根据视频ID获取视频信息
    VideoInfo getVideoById(int vodId) const;

    // 根据视频名称搜索视频
    std::vector<VideoInfo> searchVideosByName(const std::string& name) const;

    // 获取所有视频的分类统计
    std::map<std::string, int> getCategoryStatistics() const;

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
