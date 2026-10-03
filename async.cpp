#include "async.h"

#include <atomic>
#include <condition_variable>
#include <ctime>
#include <deque>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {

struct Bulk
{
    std::time_t timestamp = 0;
    std::uint64_t sequence = 0;
    std::vector<std::string> commands;
};

template <typename T>
class BlockingQueue
{
public:
    void push(T value)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push_back(std::move(value));
        }
        condition_.notify_one();
    }

    bool pop(T& value)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this] {
            return stopped_ || !queue_.empty();
        });

        if (queue_.empty())
        {
            return false;
        }

        value = std::move(queue_.front());
        queue_.pop_front();
        return true;
    }

    void stop()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopped_ = true;
        }
        condition_.notify_all();
    }

private:
    std::deque<T> queue_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool stopped_ = false;
};

class Output
{
public:
    Output()
    {
        console_thread_ = std::thread([this] { consoleLoop(); });
        file_threads_.emplace_back([this] { fileLoop(1); });
        file_threads_.emplace_back([this] { fileLoop(2); });
    }

    ~Output()
    {
        console_queue_.stop();
        file_queue_.stop();

        if (console_thread_.joinable())
        {
            console_thread_.join();
        }

        for (auto& thread : file_threads_)
        {
            if (thread.joinable())
            {
                thread.join();
            }
        }
    }

    void publish(Bulk bulk)
    {
        console_queue_.push(bulk);
        file_queue_.push(std::move(bulk));
    }

private:
    static std::string joinCommands(const Bulk& bulk)
    {
        std::string result;
        for (std::size_t i = 0; i < bulk.commands.size(); ++i)
        {
            if (i != 0)
            {
                result += ", ";
            }
            result += bulk.commands[i];
        }
        return result;
    }

    void consoleLoop()
    {
        Bulk bulk;
        while (console_queue_.pop(bulk))
        {
            std::cout << "bulk: " << joinCommands(bulk) << "\n";
        }
    }

    void fileLoop(int worker_id)
    {
        Bulk bulk;
        while (file_queue_.pop(bulk))
        {
            const std::string path = "./bulk" +
                std::to_string(bulk.timestamp) + "_" +
                std::to_string(bulk.sequence) + "_" +
                std::to_string(worker_id) + ".log";

            std::ofstream file(path);
            if (!file.is_open())
            {
                std::cerr << "Не удалось открыть файл: " << path << "\n";
                continue;
            }

            file << "bulk: " << joinCommands(bulk) << "\n";
        }
    }

    BlockingQueue<Bulk> console_queue_;
    BlockingQueue<Bulk> file_queue_;
    std::thread console_thread_;
    std::vector<std::thread> file_threads_;
};

class BulkProcessor
{
public:
    explicit BulkProcessor(std::shared_ptr<Output> output)
        : output_(std::move(output)) {}

    void setBulkSize(std::size_t size)
    {
        bulk_size_ = size;
    }

    void receiveData(const char* data, std::size_t size)
    {
        buffer_.append(data, size);

        std::size_t start = 0;

        while (true)
        {
            std::size_t end = buffer_.find('\n', start);

            if (end == std::string::npos)
            {
                break;
            }

            handleLine(buffer_.substr(start, end - start));
            start = end + 1;
        }

        buffer_.erase(0, start);
    }

    void finish()
    {
        if (!buffer_.empty())
        {
            handleLine(buffer_);
            buffer_.clear();
        }

        if (brackets_ == 0 && !commands_.empty())
        {
            completeBulk();
        }
    }

private:
    void handleLine(std::string line)
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        if (line.empty())
        {
            return;
        }

        if (line == "{")
        {
            if (brackets_ == 0 && !commands_.empty())
            {
                completeBulk();
            }

            ++brackets_;
        }
        else if (line == "}")
        {
            if (brackets_ > 0)
            {
                --brackets_;

                if (brackets_ == 0)
                {
                    completeBulk();
                }
            }
        }
        else
        {
            if (commands_.empty())
            {
                timestamp_ = std::time(nullptr);
            }

            commands_.push_back(std::move(line));

            if (brackets_ == 0 && commands_.size() >= bulk_size_)
            {
                completeBulk();
            }
        }
    }

    void completeBulk()
    {
        Bulk bulk;
        bulk.timestamp = timestamp_;
        bulk.sequence = next_sequence_++;
        bulk.commands = std::move(commands_);

        commands_.clear();
        timestamp_ = 0;

        output_->publish(std::move(bulk));
    }

    std::shared_ptr<Output> output_;
    std::size_t bulk_size_ = 0;
    std::size_t brackets_ = 0;
    std::time_t timestamp_ = 0;
    std::uint64_t next_sequence_ = 0;
    std::vector<std::string> commands_;
    std::string buffer_;
};

std::mutex& contextsMutex()
{
    static std::mutex mutex;
    return mutex;
}

std::unordered_map<void*, std::shared_ptr<BulkProcessor>>& contexts()
{
    static std::unordered_map<void*, std::shared_ptr<BulkProcessor>> map;
    return map;
}

std::shared_ptr<Output>& sharedOutput()
{
    static std::shared_ptr<Output> output = std::make_shared<Output>();
    return output;
}

} // namespace

extern "C" {

void* connect(std::size_t bulk)
{
    auto processor = std::make_shared<BulkProcessor>(sharedOutput());
    processor->setBulkSize(bulk);

    void* handle = processor.get();

    std::lock_guard<std::mutex> lock(contextsMutex());
    contexts()[handle] = std::move(processor);

    return handle;
}

void receive(void* handle, const char* data, std::size_t size)
{
    if (handle == nullptr || data == nullptr || size == 0)
    {
        return;
    }

    std::shared_ptr<BulkProcessor> processor;

    {
        std::lock_guard<std::mutex> lock(contextsMutex());
        auto it = contexts().find(handle);

        if (it == contexts().end())
        {
            return;
        }

        processor = it->second;
    }

    processor->receiveData(data, size);
}

void disconnect(void* handle)
{
    if (handle == nullptr)
    {
        return;
    }

    std::shared_ptr<BulkProcessor> processor;

    {
        std::lock_guard<std::mutex> lock(contextsMutex());
        auto it = contexts().find(handle);
        if (it == contexts().end())
        {
            return;
        }

        processor = it->second;
        contexts().erase(it);
    }

    processor->finish();
}

}