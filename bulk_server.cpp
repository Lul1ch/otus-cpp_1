#include <boost/asio.hpp>
#include <boost/asio/read_until.hpp>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <ctime>
#include <deque>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

std::atomic<int> unique_id = 0;

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
    static std::string join(const Bulk& bulk)
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
            std::cout << "bulk: " << join(bulk) << "\n";
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
                std::to_string(worker_id)
                std::to_string(unique_id) + ".log";

            unique_id.fetch_add(1);
            std::ofstream file(path);

            if (!file.is_open())
            {
                std::cerr << "Cannot open file: " << path << "\n";
                continue;
            }

            file << "bulk: " << join(bulk) << "\n";
        }
    }

    BlockingQueue<Bulk> console_queue_;
    BlockingQueue<Bulk> file_queue_;
    std::thread console_thread_;
    std::vector<std::thread> file_threads_;
};

class StaticBulkCollector
{
public:
    explicit StaticBulkCollector(std::shared_ptr<Output> output)
        : output_(std::move(output)) {}

    void setBulkSize(std::size_t size)
    {
        bulk_size_ = size;
    }

    void addCommand(std::string command)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (commands_.empty())
        {
            timestamp_ = std::time(nullptr);
        }

        commands_.push_back(std::move(command));

        if (commands_.size() >= bulk_size_)
        {
            complete();
        }
    }

    void clientConnected()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++active_clients_;
    }

    void clientDisconnected()
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (active_clients_ > 0)
        {
            --active_clients_;
        }

        // Публикуем остаток только когда клиентов больше нет.
        if (active_clients_ == 0 && !commands_.empty())
        {
            complete();
        }
    }

private:
    void complete()
    {
        Bulk bulk;
        bulk.timestamp = timestamp_;
        bulk.sequence = sequence_++;
        bulk.commands = std::move(commands_);

        commands_.clear();
        timestamp_ = 0;

        output_->publish(std::move(bulk));
    }

    std::shared_ptr<Output> output_;
    std::size_t bulk_size_ = 0;
    std::mutex mutex_;
    std::size_t active_clients_ = 0;
    std::time_t timestamp_ = 0;
    std::uint64_t sequence_ = 0;
    std::vector<std::string> commands_;
};

class ClientSession : public std::enable_shared_from_this<ClientSession>
{
public:
    ClientSession(tcp::socket socket,
                  std::shared_ptr<StaticBulkCollector> static_collector,
                  std::shared_ptr<Output> output)
        : socket_(std::move(socket)),
          static_collector_(std::move(static_collector)),
          output_(std::move(output))
    {
    }

    void start()
    {
        static_collector_->clientConnected();
        readLine();
    }

private:
    void readLine()
    {
        auto self = shared_from_this();

        asio::async_read_until(socket_, buffer_, '\n',
        [this, self](boost::system::error_code error, std::size_t bytes)
        {
            if (error)
            {
                finishDynamicBulk();
                static_collector_->clientDisconnected();
                return;
            }

            std::string received(
                asio::buffers_begin(buffer_.data()),
                asio::buffers_begin(buffer_.data()) + bytes);

            buffer_.consume(bytes);

            handleData(std::move(received));
            readLine();
        });
    }

    void handleData(std::string data)
    {
        std::size_t start = 0;

        while (true)
        {
            const std::size_t end = data.find('\n', start);

            if (end == std::string::npos)
            {
                break;
            }

            handleLine(data.substr(start, end - start));
            start = end + 1;
        }
    }

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
            ++bracket_depth_;

            if (bracket_depth_ == 1)
            {
                dynamic_timestamp_ = std::time(nullptr);
                static_collector_->complete();
            }
        }
        else if (line == "}")
        {
            if (bracket_depth_ == 0)
            {
                return;
            }

            --bracket_depth_;

            if (bracket_depth_ == 0)
            {
                completeDynamicBulk();
            }
        }
        else
        {
            if (bracket_depth_ == 0)
            {
                static_collector_->addCommand(std::move(line));
            }
            else
            {
                if (dynamic_commands_.empty())
                {
                    dynamic_timestamp_ = std::time(nullptr);
                }

                dynamic_commands_.push_back(std::move(line));
            }
        }
    }

    void completeDynamicBulk()
    {
        if (dynamic_commands_.empty())
        {
            return;
        }

        Bulk bulk;
        bulk.timestamp = dynamic_timestamp_;
        bulk.sequence = next_sequence_++;
        bulk.commands = std::move(dynamic_commands_);

        dynamic_commands_.clear();
        dynamic_timestamp_ = 0;

        output_->publish(std::move(bulk));
    }

    void finishDynamicBulk()
    {
        // Незавершённый динамический блок не публикуется.
        dynamic_commands_.clear();
        bracket_depth_ = 0;
        dynamic_timestamp_ = 0;
    }

    tcp::socket socket_;
    asio::streambuf buffer_;
    std::shared_ptr<StaticBulkCollector> static_collector_;
    std::shared_ptr<Output> output_;

    std::size_t bracket_depth_ = 0;
    std::time_t dynamic_timestamp_ = 0;
    std::uint64_t next_sequence_ = 0;
    std::vector<std::string> dynamic_commands_;
};

class Server
{
public:
    Server(asio::io_context& io_context,
           unsigned short port,
           std::size_t bulk_size)
        : output_(std::make_shared<Output>()),
          static_collector_(std::make_shared<StaticBulkCollector>(output_)),
          acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
    {
        static_collector_->setBulkSize(bulk_size);
        doAccept();
    }

private:
    void doAccept()
    {
        acceptor_.async_accept(
            [this](boost::system::error_code error, tcp::socket socket)
            {
                if (!error)
                {
                    std::make_shared<ClientSession>(
                        std::move(socket),
                        static_collector_,
                        output_)->start();
                }

                doAccept();
            });
    }

    std::shared_ptr<Output> output_;
    std::shared_ptr<StaticBulkCollector> static_collector_;
    tcp::acceptor acceptor_;
};

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <port> <bulk_size>\n";
        return 1;
    }

    try
    {
        const unsigned short port =
            static_cast<unsigned short>(std::stoul(argv[1]));

        const std::size_t bulk_size =
            static_cast<std::size_t>(std::stoull(argv[2]));

        asio::io_context io_context;

        Server server(io_context, port, bulk_size);

        io_context.run();
    }
    catch (const std::exception& error)
    {
        std::cerr << "Error: " << error.what() << "\n";
        return 1;
    }

    return 0;
}