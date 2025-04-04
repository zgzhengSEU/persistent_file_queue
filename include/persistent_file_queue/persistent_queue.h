#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace persistent_file_queue {

class PersistentQueue {
public:
    // 默认配置
    static constexpr const char* DEFAULT_STORAGE_DIR = "storage";  // 默认存储目录
    static constexpr const char* DEFAULT_LOG_DIR = "logs";        // 默认日志目录
    static constexpr size_t DEFAULT_BLOCK_SIZE = 64 * 1024 * 1024; // 64MB

    // 构造函数，允许用户配置存储路径和日志路径
    explicit PersistentQueue(
        std::string_view queue_name,                                    // 队列名称
        std::string_view storage_dir = DEFAULT_STORAGE_DIR,            // 存储目录
        size_t block_size = DEFAULT_BLOCK_SIZE,                        // 块大小
        std::string_view log_dir = DEFAULT_LOG_DIR                     // 日志目录
    );

    ~PersistentQueue();

    // 禁止拷贝和移动
    PersistentQueue(const PersistentQueue&) = delete;
    PersistentQueue& operator=(const PersistentQueue&) = delete;
    PersistentQueue(PersistentQueue&&) = delete;
    PersistentQueue& operator=(PersistentQueue&&) = delete;

    // 入队操作
    bool Enqueue(const std::vector<std::byte>& data);

    // 出队操作
    std::optional<std::vector<std::byte>> Dequeue();

    // 获取队列中数据项的数量
    size_t Size() const;

    // 获取队列占用的总字节数（包括元数据）
    size_t TotalBytes() const;

    // 检查队列是否为空
    bool Empty() const;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace persistent_file_queue 