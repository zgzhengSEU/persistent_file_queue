#include "persistent_file_queue/persistent_queue.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <span>
#include <stdexcept>
#include <vector>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

// 简单的校验和计算：所有字节相加
std::byte CalculateChecksum(const std::byte* data, size_t length) {
    std::byte sum{0};
    for (size_t i = 0; i < length; ++i) {
        sum = static_cast<std::byte>(static_cast<uint8_t>(sum) + 
                                    static_cast<uint8_t>(data[i]));
    }
    return sum;
}

namespace persistent_file_queue {

// 文件头部结构
struct QueueHeader {
    uint64_t head;       // 队列头位置
    uint64_t tail;       // 队列尾位置
    uint64_t capacity;   // 队列容量
    uint64_t size;       // 当前队列大小（字节数）
    uint64_t count;      // 当前队列中数据项的数量
    uint64_t block_size; // 块大小
    uint64_t max_size;   // 最大文件大小
    uint64_t write_pos;  // 当前写入位置
    uint64_t read_pos;   // 当前读取位置
    uint64_t magic;      // 魔数，用于验证文件格式
    uint64_t version;    // 版本号
    std::byte checksum;  // 头部校验和
};

class PersistentQueue::Impl {
public:
    Impl(std::string_view queue_name, std::string_view storage_dir, size_t block_size, std::string_view log_dir)
        : block_size_(block_size) {
        // 处理存储路径
        fs::path storage_path = fs::path(storage_dir) / (std::string(queue_name) + ".dat");
        file_path_ = storage_path.string();
        
        // 处理日志路径
        std::string effective_log_path;
        if (log_dir.empty()) {
            effective_log_path = DEFAULT_LOG_DIR;
        } else {
            effective_log_path = log_dir;
        }
        
        // 确保存储目录和日志目录存在
        try {
            fs::create_directories(storage_path.parent_path());
            fs::create_directories(effective_log_path);
        } catch (const fs::filesystem_error& e) {
            spdlog::error("Failed to create directories: {}", e.what());
            throw;
        }
        
        // 使用固定的日志文件名
        std::string log_file = fs::path(effective_log_path) / "persistent_queue.log";
        
        // 初始化日志记录器
        logger_ = spdlog::get("persistent_queue");
        if (!logger_) {
            // 创建滚动日志文件，每个文件最大 1GB，不限制文件数量
            const size_t max_file_size = 1024 * 1024 * 1024;  // 1GB
            logger_ = spdlog::rotating_logger_mt("persistent_queue", log_file, max_file_size, 0);
            // 设置日志格式
            logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%t] [%v]");
            // 设置日志级别
            logger_->set_level(spdlog::level::debug);
            // 设置日志刷新策略：立即刷新到磁盘
            logger_->flush_on(spdlog::level::info);
        }
        logger_->info("PersistentQueue created with file: {}", file_path_);
        
        // 打开或创建文件
        OpenFile();
        
        // 获取文件大小
        const size_t file_size = GetFileSize();
        
        // 如果是新文件，初始化它
        if (file_size == 0) {
            logger_->info("Creating new queue file: {}", file_path_);
            Initialize();
        } else {
            logger_->info("Opening existing queue file: {}", file_path_);
            MapHeaderBlock();
            RecoverFromFile();
        }
    }

    ~Impl() {
        // 确保头部信息写入磁盘
        FlushHeader();
        
        // 解除所有块的映射
        for (auto& [_, block] : mapped_blocks_) {
            UnmapBlock(block);
        }
        if (file_handle_ != InvalidHandle) {
            CloseFile();
        }
        logger_->info("PersistentQueue destroyed");
        spdlog::drop("persistent_queue");  // 关闭日志记录器
    }

    bool Enqueue(const std::vector<std::byte>& data) {
        logger_->debug("Enqueue data with size: {}", data.size());
        std::scoped_lock lock(mutex_);
        
        // 计算需要写入的总大小（数据大小 + 大小字段 + 校验和）
        const size_t total_size = sizeof(uint32_t) + data.size() + sizeof(std::byte);
        
        // 检查是否有足够的空间
        if (header_->size + total_size > header_->capacity) {
            if (header_->capacity >= header_->max_size) {
                // 如果达到最大大小，检查是否有可回收的空间
                if (CanRecycleSpace(total_size)) {
                    // 更新读取位置，回收空间
                    UpdateReadPosition();
                } else {
                    spdlog::warn("Queue is full and cannot recycle space");
                    return false;  // 队列已满且无法回收空间
                }
            } else {
                // 扩展文件
                ExpandFile();
            }
        }

        // 确保写入位置块已映射
        EnsureBlockMapped(header_->write_pos / block_size_);
        
        // 写入数据大小
        uint32_t data_size = static_cast<uint32_t>(data.size());
        std::byte* write_pos = GetBlockPtr(header_->write_pos);
        std::memcpy(write_pos, &data_size, sizeof(uint32_t));
        
        // 写入实际数据
        write_pos += sizeof(uint32_t);
        std::memcpy(write_pos, data.data(), data.size());
        
        // 计算并写入校验和
        write_pos += data.size();
        std::byte checksum = CalculateChecksum(data.data(), data.size());
        *write_pos = checksum;
        
        // 确保数据写入磁盘
        FlushBlock(header_->write_pos / block_size_);
        
        // 更新队列状态
        header_->write_pos = (header_->write_pos + total_size) % header_->capacity;
        header_->size += total_size;
        header_->count += 1;  // 增加数据项计数
        
        // 更新头部信息
        FlushHeader();

        logger_->debug("Data enqueued successfully, new size: {}, count: {}", 
                      header_->size, header_->count);
        return true;
    }

    std::optional<std::vector<std::byte>> Dequeue() {
        logger_->debug("Attempting to dequeue data");
        std::scoped_lock lock(mutex_);
        
        if (header_->count == 0) {
            spdlog::debug("Queue is empty");
            return std::nullopt;  // 队列为空
        }

        // 确保读取位置块已映射
        EnsureBlockMapped(header_->read_pos / block_size_);
        
        // 读取数据大小
        uint32_t data_size;
        std::memcpy(&data_size, GetBlockPtr(header_->read_pos), sizeof(uint32_t));
        
        // 计算总大小（数据大小 + 大小字段 + 校验和）
        const size_t total_size = sizeof(uint32_t) + data_size + sizeof(std::byte);
        
        // 分配空间并读取数据
        std::vector<std::byte> data(data_size);
        std::memcpy(data.data(), GetBlockPtr(header_->read_pos + sizeof(uint32_t)), data_size);
        
        // 读取并验证校验和
        std::byte stored_checksum = *(GetBlockPtr(header_->read_pos + sizeof(uint32_t) + data_size));
        std::byte calculated_checksum = CalculateChecksum(data.data(), data_size);
        
        if (stored_checksum != calculated_checksum) {
            spdlog::error("Data corruption detected: checksum mismatch");
            throw std::runtime_error("Data corruption detected: checksum mismatch");
        }
        
        // 更新队列状态
        header_->read_pos = (header_->read_pos + total_size) % header_->capacity;
        header_->size -= total_size;
        header_->count -= 1;  // 减少数据项计数
        
        // 更新头部信息
        FlushHeader();

        logger_->debug("Data dequeued successfully, remaining size: {}, count: {}", 
                      header_->size, header_->count);
        return data;
    }

    size_t Size() const {
        std::scoped_lock lock(mutex_);
        size_t count = header_->count;
        logger_->debug("Current queue count: {}", count);
        return count;
    }

    size_t TotalBytes() const {
        std::scoped_lock lock(mutex_);
        size_t size = header_->size;
        logger_->debug("Current queue size in bytes: {}", size);
        return size;
    }

    bool Empty() const {
        std::scoped_lock lock(mutex_);
        return header_->count == 0;
    }

private:
#ifdef _WIN32
    using FileHandle = HANDLE;
    static constexpr FileHandle InvalidHandle = INVALID_HANDLE_VALUE;
#else
    using FileHandle = int;
    static constexpr FileHandle InvalidHandle = -1;
#endif

    static constexpr uint64_t MAGIC_NUMBER = 0xDEADBEEFCAFEBABE;
    static constexpr uint64_t CURRENT_VERSION = 1;

    struct MappedBlock {
        std::byte* data;
        size_t ref_count;
    };

    void Initialize() {
        // 计算初始块数（至少4个块，每个块64MB）
        const size_t initial_blocks = std::max<size_t>(4, (1ULL << 30) / block_size_); // 1GB / block_size
        const size_t initial_size = initial_blocks * block_size_;

        // 调整文件大小
        ResizeFile(initial_size);

        // 映射头部块（使用单独的头部块）
        MapHeaderBlock();
        
        // 初始化头部
        InitializeNewFile(initial_size);
    }

    void InitializeNewFile(size_t initial_size) {
        // 初始化头部字段
        header_->head = block_size_;
        header_->tail = block_size_;
        header_->capacity = initial_size;
        header_->size = 0;
        header_->count = 0;
        header_->block_size = block_size_;
        header_->max_size = 1ULL << 30; // 1GB
        header_->write_pos = block_size_;
        header_->read_pos = block_size_;
        header_->magic = MAGIC_NUMBER;
        header_->version = CURRENT_VERSION;
        
        // 计算头部校验和
        header_->checksum = CalculateChecksum(
            reinterpret_cast<const std::byte*>(header_),
            sizeof(QueueHeader) - sizeof(std::byte)
        );
        
        // 确保头部信息写入磁盘
        FlushHeader();
        
        logger_->info("New file initialized with size: {}, block size: {}", 
                     initial_size, block_size_);
    }

    void RecoverFromFile() {
        // 验证文件头部
        if (header_->magic != MAGIC_NUMBER) {
            throw std::runtime_error("Invalid file format: magic number mismatch");
        }

        if (header_->version != CURRENT_VERSION) {
            throw std::runtime_error("Unsupported file version");
        }

        if (header_->block_size != block_size_) {
            throw std::runtime_error("Block size mismatch");
        }

        // 验证队列状态
        if (header_->size > header_->capacity) {
            throw std::runtime_error("Invalid queue size");
        }

        if (header_->read_pos >= header_->capacity || header_->write_pos >= header_->capacity) {
            throw std::runtime_error("Invalid read/write positions");
        }

        // 验证数据完整性
        VerifyDataIntegrity();
    }

    void VerifyDataIntegrity() {
        if (header_->size == 0) {
            return;  // 空队列，无需验证
        }

        size_t current_pos = header_->read_pos;
        size_t remaining_size = header_->size;

        while (remaining_size > 0) {
            // 确保当前块已映射
            EnsureBlockMapped(current_pos / block_size_);

            // 读取数据大小
            uint32_t data_size;
            std::memcpy(&data_size, GetBlockPtr(current_pos), sizeof(uint32_t));

            // 计算总大小
            const size_t total_size = sizeof(uint32_t) + data_size + sizeof(std::byte);

            if (total_size > remaining_size) {
                throw std::runtime_error("Data corruption: invalid data size");
            }

            // 验证校验和
            std::byte stored_checksum = *(GetBlockPtr(current_pos + sizeof(uint32_t) + data_size));
            std::byte calculated_checksum = CalculateChecksum(
                GetBlockPtr(current_pos + sizeof(uint32_t)),
                data_size
            );

            if (stored_checksum != calculated_checksum) {
                throw std::runtime_error("Data corruption: checksum mismatch");
            }

            // 移动到下一个数据项
            current_pos = (current_pos + total_size) % header_->capacity;
            remaining_size -= total_size;
        }
    }

    bool CanRecycleSpace(size_t required_size) {
        // 检查是否有足够的可回收空间
        if (header_->read_pos > block_size_) {
            // 如果读取位置不在文件开头，说明有可回收空间
            return true;
        }
        
        // 检查写入位置是否已经绕回
        if (header_->write_pos < header_->read_pos) {
            // 如果写入位置在读取位置之前，说明已经绕回
            return (header_->read_pos - header_->write_pos) >= required_size;
        }
        
        return false;
    }

    void UpdateReadPosition() {
        // 更新读取位置到文件开头
        header_->read_pos = block_size_;
        FlushHeader();
    }

    void ExpandFile() {
        // 计算新的文件大小（每次扩展一倍，但不超过最大大小）
        size_t new_size = std::min(header_->capacity * 2, header_->max_size);
        
        // 调整文件大小
        ResizeFile(new_size);
        
        // 更新容量
        header_->capacity = new_size;
        FlushHeader();
    }

    void OpenFile() {
#ifdef _WIN32
        file_handle_ = CreateFileA(
            file_path_.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );
        if (file_handle_ == InvalidHandle) {
            throw std::runtime_error("Failed to open queue file");
        }
#else
        file_handle_ = open(file_path_.c_str(), O_RDWR | O_CREAT, 0644);
        if (file_handle_ == InvalidHandle) {
            throw std::runtime_error("Failed to open queue file");
        }
#endif
    }

    void CloseFile() {
#ifdef _WIN32
        CloseHandle(file_handle_);
#else
        close(file_handle_);
#endif
    }

    size_t GetFileSize() {
#ifdef _WIN32
        LARGE_INTEGER size;
        if (!GetFileSizeEx(file_handle_, &size)) {
            throw std::runtime_error("Failed to get file size");
        }
        return static_cast<size_t>(size.QuadPart);
#else
        struct stat st;
        if (fstat(file_handle_, &st) == -1) {
            throw std::runtime_error("Failed to get file size");
        }
        return static_cast<size_t>(st.st_size);
#endif
    }

    void ResizeFile(size_t new_size) {
#ifdef _WIN32
        LARGE_INTEGER size;
        size.QuadPart = new_size;
        if (!SetFilePointerEx(file_handle_, size, nullptr, FILE_BEGIN) ||
            !SetEndOfFile(file_handle_)) {
            throw std::runtime_error("Failed to resize file");
        }
#else
        if (ftruncate(file_handle_, new_size) == -1) {
            throw std::runtime_error("Failed to resize file");
        }
#endif
    }

    void MapHeaderBlock() {
        // 头部块固定为4KB
        const size_t header_block_size = 4096;
        
#ifdef _WIN32
        HANDLE mapping = CreateFileMapping(
            file_handle_,
            nullptr,
            PAGE_READWRITE,
            0,
            header_block_size,
            nullptr
        );
        if (mapping == nullptr) {
            throw std::runtime_error("Failed to create file mapping");
        }

        void* data = MapViewOfFile(
            mapping,
            FILE_MAP_ALL_ACCESS,
            0,
            0,
            header_block_size
        );
        CloseHandle(mapping);
        if (data == nullptr) {
            throw std::runtime_error("Failed to map view of file");
        }
#else
        void* data = mmap(
            nullptr,
            header_block_size,
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            file_handle_,
            0
        );
        if (data == MAP_FAILED) {
            throw std::runtime_error("Failed to memory map header block");
        }
#endif
        header_ = reinterpret_cast<QueueHeader*>(data);
        logger_->debug("Header block mapped at address: {}", static_cast<void*>(header_));
    }

    void UnmapBlock(const MappedBlock& block) {
#ifdef _WIN32
        UnmapViewOfFile(block.data);
#else
        munmap(block.data, block_size_);
#endif
    }

    void EnsureBlockMapped(size_t block_index) {
        MapBlock(block_index);
    }

    void FlushBlock(size_t block_index) {
        // 跳过头部块
        if (block_index == 0) return;
        
#ifdef _WIN32
        FlushViewOfFile(mapped_blocks_[block_index].data, block_size_);
#else
        msync(mapped_blocks_[block_index].data, block_size_, MS_SYNC);
#endif
    }

    std::byte* GetBlockPtr(uint64_t offset) {
        size_t block_index = offset / block_size_;
        size_t block_offset = offset % block_size_;
        return mapped_blocks_[block_index].data + block_offset;
    }

    void FlushHeader() {
#ifdef _WIN32
        FlushViewOfFile(header_, sizeof(QueueHeader));
#else
        msync(header_, sizeof(QueueHeader), MS_SYNC);
#endif
    }

    void MapBlock(size_t block_index) {
        if (mapped_blocks_.find(block_index) == mapped_blocks_.end()) {
#ifdef _WIN32
            HANDLE mapping = CreateFileMapping(
                file_handle_,
                nullptr,
                PAGE_READWRITE,
                0,
                block_size_,
                nullptr
            );
            if (mapping == nullptr) {
                throw std::runtime_error("Failed to create file mapping");
            }

            void* data = MapViewOfFile(
                mapping,
                FILE_MAP_ALL_ACCESS,
                0,
                block_index * block_size_,
                block_size_
            );
            CloseHandle(mapping);
            if (data == nullptr) {
                throw std::runtime_error("Failed to map view of file");
            }
#else
            void* data = mmap(
                nullptr,
                block_size_,
                PROT_READ | PROT_WRITE,
                MAP_SHARED,
                file_handle_,
                block_index * block_size_
            );
            if (data == MAP_FAILED) {
                throw std::runtime_error("Failed to memory map block");
            }
#endif
            mapped_blocks_[block_index] = {static_cast<std::byte*>(data), 1};
        } else {
            mapped_blocks_[block_index].ref_count++;
        }
    }

    std::string file_path_;
    size_t block_size_;
    FileHandle file_handle_;
    QueueHeader* header_;
    std::map<size_t, MappedBlock> mapped_blocks_;
    mutable std::mutex mutex_;
    std::shared_ptr<spdlog::logger> logger_;
};

// PersistentQueue 实现
PersistentQueue::PersistentQueue(std::string_view queue_name, 
                               std::string_view storage_dir,
                               size_t block_size,
                               std::string_view log_dir)
    : pimpl_(std::make_unique<Impl>(queue_name, storage_dir, block_size, log_dir)) {}

PersistentQueue::~PersistentQueue() = default;

bool PersistentQueue::Enqueue(const std::vector<std::byte>& data) {
    return pimpl_->Enqueue(data);
}

std::optional<std::vector<std::byte>> PersistentQueue::Dequeue() {
    return pimpl_->Dequeue();
}

size_t PersistentQueue::Size() const {
    return pimpl_->Size();
}

size_t PersistentQueue::TotalBytes() const {
    return pimpl_->TotalBytes();
}

bool PersistentQueue::Empty() const {
    return pimpl_->Empty();
}

} // namespace persistent_file_queue 