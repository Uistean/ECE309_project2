#include "core/sentinel_scanner.h"
#include <utility>
#include <algorithm>
SentinelScanner:: SentinelScanner(std::string sentinel): sentinel_(std::move(sentinel)){}

SentinelScanner:: Out SentinelScanner::feed(std:: string_view chunk){
    std::string buf = pending_ + std::string(chunk);

    std::size_t pos = buf.find(sentinel_);
    if (pos != std::string::npos){
        pending_.clear();
        return {buf.substr(0,pos),true};
    }

    std:: size_t keep = std::min(buf.size(), sentinel_.size() - 1);
    std:: size_t safe_len = buf.size() -keep;

    pending_ = buf.substr(safe_len);
    return {buf.substr(0,safe_len), false};
}
SentinelScanner:: Out SentinelScanner::flush(){
    std:: string result = pending_;
    pending_.clear();
    return{result, false};
}