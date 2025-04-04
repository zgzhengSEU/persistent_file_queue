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
    explicit PersistentQueue(std::string_view file_path, size_t block_size = 64 * 1024 * 1024);
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

    // 获取队列大小
    size_t Size() const;

    // 检查队列是否为空
    bool Empty() const;

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace persistent_file_queue 