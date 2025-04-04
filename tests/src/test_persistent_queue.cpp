#include "persistent_file_queue/persistent_queue.h"
#include <gtest/gtest.h>
#include <filesystem>
#include <thread>
#include <vector>
#include <algorithm>

using namespace persistent_file_queue;

class PersistentQueueTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建临时文件用于测试
        queue_file_ = std::filesystem::temp_directory_path() / "test_queue.dat";
    }

    void TearDown() override {
        // 清理测试文件
        std::filesystem::remove(queue_file_);
    }

    std::filesystem::path queue_file_;

    // 辅助函数：比较两个字节向量是否相等
    bool CompareByteVectors(const std::vector<std::byte>& a, const std::vector<std::byte>& b) {
        if (a.size() != b.size()) return false;
        return std::equal(a.begin(), a.end(), b.begin());
    }
};

TEST_F(PersistentQueueTest, BasicOperations) {
    PersistentQueue queue(queue_file_.string());
    
    // 测试入队
    std::vector<std::byte> data1 = {
        std::byte{1}, std::byte{2}, std::byte{3}, 
        std::byte{4}, std::byte{5}
    };
    EXPECT_TRUE(queue.Enqueue(data1));
    
    // 测试出队
    auto result = queue.Dequeue();
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(CompareByteVectors(result.value(), data1));
    EXPECT_TRUE(queue.Empty());
}

TEST_F(PersistentQueueTest, Persistence) {
    std::vector<std::byte> data = {
        std::byte{1}, std::byte{2}, std::byte{3}, 
        std::byte{4}, std::byte{5}
    };

    {
        // 创建队列并写入数据
        PersistentQueue queue(queue_file_.string());
        EXPECT_TRUE(queue.Enqueue(data));
    }
    
    {
        // 重新打开队列并读取数据
        PersistentQueue queue(queue_file_.string());
        auto result = queue.Dequeue();
        EXPECT_TRUE(result.has_value());
        EXPECT_TRUE(CompareByteVectors(result.value(), data));
        EXPECT_TRUE(queue.Empty());
    }
}

TEST_F(PersistentQueueTest, ConcurrentAccess) {
    PersistentQueue queue(queue_file_.string());
    
    // 创建多个线程同时操作队列
    std::vector<std::thread> threads;
    const int num_threads = 4;
    const int num_operations = 1000;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&queue, i]() {
            for (int j = 0; j < num_operations; ++j) {
                std::vector<std::byte> data = {
                    std::byte{static_cast<uint8_t>(i)}, 
                    std::byte{static_cast<uint8_t>(j)}
                };
                queue.Enqueue(data);
                
                auto result = queue.Dequeue();
                if (result.has_value()) {
                    EXPECT_EQ(result.value().size(), 2);
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_TRUE(queue.Empty());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 