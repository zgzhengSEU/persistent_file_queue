#include "persistent_file_queue/persistent_queue.h"
#include <gtest/gtest.h>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace persistent_file_queue;

class PersistentQueueTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 确保测试目录不存在
        if (fs::exists(storage_dir_)) {
            fs::remove_all(storage_dir_);
        }
        if (fs::exists(log_dir_)) {
            fs::remove_all(log_dir_);
        }
        
        // 创建测试目录
        fs::create_directories(storage_dir_);
        fs::create_directories(log_dir_);
    }

    void TearDown() override {
        // 清理测试目录
        if (fs::exists(storage_dir_)) {
            fs::remove_all(storage_dir_);
        }
        if (fs::exists(log_dir_)) {
            fs::remove_all(log_dir_);
        }
    }

    const std::string queue_name_ = "test_queue";
    const std::string storage_dir_ = "test_storage";
    const std::string log_dir_ = "test_logs";
};

// 辅助函数：将字符串转换为字节向量
std::vector<std::byte> StringToBytes(const std::string& str) {
    std::vector<std::byte> bytes;
    bytes.reserve(str.size());
    for (char c : str) {
        bytes.push_back(static_cast<std::byte>(c));
    }
    return bytes;
}

// 辅助函数：将字节向量转换为字符串
std::string BytesToString(const std::vector<std::byte>& bytes) {
    std::string str;
    str.reserve(bytes.size());
    for (std::byte b : bytes) {
        str.push_back(static_cast<char>(b));
    }
    return str;
}

// 计算队列中数据项的总大小（包括元数据）
size_t CalculateTotalSize(size_t data_size) {
    return sizeof(uint32_t) + data_size + sizeof(std::byte);  // 大小字段 + 数据 + 校验和
}

// 测试基本操作
TEST_F(PersistentQueueTest, BasicOperations) {
    PersistentQueue queue(queue_name_, storage_dir_, 64 * 1024 * 1024, log_dir_);
    
    // 测试空队列
    EXPECT_TRUE(queue.Empty());
    EXPECT_EQ(queue.Size(), 0);
    EXPECT_EQ(queue.TotalBytes(), 0);
    
    // 测试入队
    std::string test_str = "Hello, World!";
    std::vector<std::byte> data = StringToBytes(test_str);
    EXPECT_TRUE(queue.Enqueue(data));
    EXPECT_FALSE(queue.Empty());
    EXPECT_EQ(queue.Size(), 1);  // 现在 Size() 返回数据项数量
    EXPECT_EQ(queue.TotalBytes(), CalculateTotalSize(data.size()));  // 使用 TotalBytes() 获取字节数
    
    // 测试出队
    auto result = queue.Dequeue();
    ASSERT_TRUE(result.has_value());
    std::string result_str = BytesToString(result.value());
    EXPECT_EQ(result_str, test_str);
    EXPECT_TRUE(queue.Empty());
    EXPECT_EQ(queue.Size(), 0);
    EXPECT_EQ(queue.TotalBytes(), 0);
}

// 测试多个字符串
TEST_F(PersistentQueueTest, MultipleStrings) {
    PersistentQueue queue(queue_name_, storage_dir_, 64 * 1024 * 1024, log_dir_);
    
    // 准备测试数据
    std::vector<std::string> test_strings = {
        "Hello",
        "World",
        "This is a test",
        "Another string",
        "Last one"
    };
    
    // 入队所有字符串
    size_t total_bytes = 0;
    for (const auto& str : test_strings) {
        std::vector<std::byte> data = StringToBytes(str);
        EXPECT_TRUE(queue.Enqueue(data));
        total_bytes += CalculateTotalSize(data.size());
    }
    EXPECT_EQ(queue.Size(), test_strings.size());  // 现在 Size() 返回数据项数量
    EXPECT_EQ(queue.TotalBytes(), total_bytes);    // 使用 TotalBytes() 获取字节数
    
    // 出队并验证所有字符串
    for (const auto& expected_str : test_strings) {
        auto result = queue.Dequeue();
        ASSERT_TRUE(result.has_value());
        std::string result_str = BytesToString(result.value());
        EXPECT_EQ(result_str, expected_str);
    }
    
    EXPECT_TRUE(queue.Empty());
    EXPECT_EQ(queue.Size(), 0);
    EXPECT_EQ(queue.TotalBytes(), 0);
}

// 测试长字符串
TEST_F(PersistentQueueTest, LongString) {
    PersistentQueue queue(queue_name_, storage_dir_, 64 * 1024 * 1024, log_dir_);
    
    // 生成一个长字符串
    std::string long_str(1024 * 1024, 'A');  // 1MB 的字符串
    std::vector<std::byte> data = StringToBytes(long_str);
    
    // 入队
    EXPECT_TRUE(queue.Enqueue(data));
    
    // 出队
    auto result = queue.Dequeue();
    ASSERT_TRUE(result.has_value());
    std::string result_str = BytesToString(result.value());
    EXPECT_EQ(result_str, long_str);
}

// 测试特殊字符
TEST_F(PersistentQueueTest, SpecialCharacters) {
    PersistentQueue queue(queue_name_, storage_dir_, 64 * 1024 * 1024, log_dir_);
    
    // 包含各种特殊字符的字符串
    std::string special_str = "Hello\nWorld\tTest\r\n\0\xFF\xFE";
    std::vector<std::byte> data = StringToBytes(special_str);
    
    // 入队
    EXPECT_TRUE(queue.Enqueue(data));
    
    // 出队
    auto result = queue.Dequeue();
    ASSERT_TRUE(result.has_value());
    std::string result_str = BytesToString(result.value());
    EXPECT_EQ(result_str, special_str);
}

// 测试中文字符
TEST_F(PersistentQueueTest, ChineseCharacters) {
    PersistentQueue queue(queue_name_, storage_dir_, 64 * 1024 * 1024, log_dir_);
    
    // 包含中文字符的字符串
    std::string chinese_str = "你好，世界！这是一个测试。";
    std::vector<std::byte> data = StringToBytes(chinese_str);
    
    // 入队
    EXPECT_TRUE(queue.Enqueue(data));
    
    // 出队
    auto result = queue.Dequeue();
    ASSERT_TRUE(result.has_value());
    std::string result_str = BytesToString(result.value());
    EXPECT_EQ(result_str, chinese_str);
}

// 测试混合字符
TEST_F(PersistentQueueTest, MixedCharacters) {
    PersistentQueue queue(queue_name_, storage_dir_, 64 * 1024 * 1024, log_dir_);
    
    // 包含各种字符的字符串
    std::string mixed_str = "Hello 你好！\nThis is a test 这是一个测试。\tSpecial: \0\xFF";
    std::vector<std::byte> data = StringToBytes(mixed_str);
    
    // 入队
    EXPECT_TRUE(queue.Enqueue(data));
    
    // 出队
    auto result = queue.Dequeue();
    ASSERT_TRUE(result.has_value());
    std::string result_str = BytesToString(result.value());
    EXPECT_EQ(result_str, mixed_str);
}

// 测试默认路径
TEST_F(PersistentQueueTest, DefaultPaths) {
    // 清理默认目录
    if (fs::exists(PersistentQueue::DEFAULT_STORAGE_DIR)) {
        fs::remove_all(PersistentQueue::DEFAULT_STORAGE_DIR);
    }
    if (fs::exists(PersistentQueue::DEFAULT_LOG_DIR)) {
        fs::remove_all(PersistentQueue::DEFAULT_LOG_DIR);
    }

    // 使用默认的存储路径和日志路径
    PersistentQueue queue("default_queue");
    
    // 测试基本操作
    EXPECT_TRUE(queue.Empty());
    EXPECT_EQ(queue.Size(), 0);
    
    std::string test_str = "Test with default paths";
    std::vector<std::byte> data = StringToBytes(test_str);
    EXPECT_TRUE(queue.Enqueue(data));
    
    auto result = queue.Dequeue();
    ASSERT_TRUE(result.has_value());
    std::string result_str = BytesToString(result.value());
    EXPECT_EQ(result_str, test_str);

    // 清理默认目录
    if (fs::exists(PersistentQueue::DEFAULT_STORAGE_DIR)) {
        fs::remove_all(PersistentQueue::DEFAULT_STORAGE_DIR);
    }
    if (fs::exists(PersistentQueue::DEFAULT_LOG_DIR)) {
        fs::remove_all(PersistentQueue::DEFAULT_LOG_DIR);
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 