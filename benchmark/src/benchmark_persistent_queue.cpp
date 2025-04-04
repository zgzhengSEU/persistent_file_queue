#include "persistent_file_queue/persistent_queue.h"
#include <benchmark/benchmark.h>
#include <random>
#include <filesystem>

namespace fs = std::filesystem;

// 获取临时目录路径
std::string GetTempFilePath() {
    static const std::string temp_dir = fs::temp_directory_path().string();
    return temp_dir + "/benchmark_queue.dat";
}

// 生成随机数据
std::vector<std::byte> GenerateRandomData(size_t size) {
    std::vector<std::byte> data(size);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    for (auto& byte : data) {
        byte = static_cast<std::byte>(dis(gen));
    }
    return data;
}

// 基准测试：入队操作
static void BM_Enqueue(benchmark::State& state) {
    const std::string test_file = GetTempFilePath();
    const size_t data_size = state.range(0);
    auto data = GenerateRandomData(data_size);
    
    for (auto _ : state) {
        state.PauseTiming();
        if (fs::exists(test_file)) {
            fs::remove(test_file);
        }
        persistent_file_queue::PersistentQueue queue(test_file);
        state.ResumeTiming();
        
        queue.Enqueue(data);
    }
    
    if (fs::exists(test_file)) {
        fs::remove(test_file);
    }
}

// 基准测试：出队操作
static void BM_Dequeue(benchmark::State& state) {
    const std::string test_file = GetTempFilePath();
    const size_t data_size = state.range(0);
    auto data = GenerateRandomData(data_size);
    
    for (auto _ : state) {
        state.PauseTiming();
        if (fs::exists(test_file)) {
            fs::remove(test_file);
        }
        persistent_file_queue::PersistentQueue queue(test_file);
        queue.Enqueue(data);
        state.ResumeTiming();
        
        auto result = queue.Dequeue();
        benchmark::DoNotOptimize(result);
    }
    
    if (fs::exists(test_file)) {
        fs::remove(test_file);
    }
}

// 基准测试：批量操作
static void BM_BatchOperations(benchmark::State& state) {
    const std::string test_file = GetTempFilePath();
    const size_t data_size = state.range(0);
    const size_t batch_size = state.range(1);
    auto data = GenerateRandomData(data_size);
    
    for (auto _ : state) {
        state.PauseTiming();
        if (fs::exists(test_file)) {
            fs::remove(test_file);
        }
        persistent_file_queue::PersistentQueue queue(test_file);
        state.ResumeTiming();
        
        // 批量入队
        for (size_t i = 0; i < batch_size; ++i) {
            queue.Enqueue(data);
        }
        
        // 批量出队
        for (size_t i = 0; i < batch_size; ++i) {
            auto result = queue.Dequeue();
            benchmark::DoNotOptimize(result);
        }
    }
    
    if (fs::exists(test_file)) {
        fs::remove(test_file);
    }
}

// 注册基准测试
BENCHMARK(BM_Enqueue)
    ->Arg(64)      // 64字节
    ->Arg(1024)    // 1KB
    ->Arg(65536)   // 64KB
    ->Arg(1048576) // 1MB
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_Dequeue)
    ->Arg(64)      // 64字节
    ->Arg(1024)    // 1KB
    ->Arg(65536)   // 64KB
    ->Arg(1048576) // 1MB
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_BatchOperations)
    ->Args({64, 1000})      // 64字节，1000次操作
    ->Args({1024, 1000})    // 1KB，1000次操作
    ->Args({65536, 100})    // 64KB，100次操作
    ->Args({1048576, 10})   // 1MB，10次操作
    ->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN(); 