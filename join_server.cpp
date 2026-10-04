#include <boost/asio.hpp>
#include <boost/asio/read_until.hpp>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <deque>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
using ErrorCode = boost::system::error_code;

using Table = std::map<int, std::string>;

class JoinServer
{
public:
    explicit JoinServer(asio::io_context& io_context)
        : strand_(asio::make_strand(io_context)) {}

    std::string handleCommand(const std::string& line)
    {
        std::istringstream stream(line);

        std::string command;
        if (!(stream >> command))
        {
            return "ERR empty command\n";
        }

        if (command == "INSERT")
        {
            return handleInsert(stream);
        }

        if (command == "TRUNCATE")
        {
            return handleTruncate(stream);
        }

        if (command == "INTERSECTION")
        {
            return formatResult(intersection());
        }

        if (command == "SYMMETRIC_DIFFERENCE")
        {
            return formatResult(symmetricDifference());
        }

        return "ERR unknown command\n";
    }

private:
    struct JoinResultRow
    {
        int id;
        std::string a_name;
        std::string b_name;
    };

    static bool readTableName(std::istringstream& stream, std::string& table)
    {
        if (!(stream >> table))
        {
            return false;
        }

        return table == "A" || table == "B";
    }

    std::string handleInsert(std::istringstream& stream)
    {
        std::string table;
        int id = 0;
        std::string name;

        if (!readTableName(stream, table) ||
            !(stream >> id) ||
            !(stream >> name))
        {
            return "ERR invalid arguments\n";
        }

        std::string extra;
        if (stream >> extra)
        {
            return "ERR invalid arguments\n";
        }

        Table& target = (table == "A") ? table_a_ : table_b_;

        if (target.find(id) != target.end())
        {
            return "ERR duplicate " + std::to_string(id) + "\n";
        }

        target.emplace(id, std::move(name));
        return "OK\n";
    }

    std::string handleTruncate(std::istringstream& stream)
    {
        std::string table;

        if (!readTableName(stream, table))
        {
            return "ERR invalid table\n";
        }

        std::string extra;
        if (stream >> extra)
        {
            return "ERR invalid arguments\n";
        }

        if (table == "A")
        {
            table_a_.clear();
        }
        else
        {
            table_b_.clear();
        }

        return "OK\n";
    }

    std::vector<JoinResultRow> intersection() const
    {
        std::vector<JoinResultRow> result;

        auto a_it = table_a_.begin();
        auto b_it = table_b_.begin();

        while (a_it != table_a_.end() && b_it != table_b_.end())
        {
            if (a_it->first < b_it->first)
            {
                ++a_it;
            }
            else if (b_it->first < a_it->first)
            {
                ++b_it;
            }
            else
            {
                result.push_back({a_it->first, a_it->second, b_it->second});
                ++a_it;
                ++b_it;
            }
        }

        return result;
    }

    std::vector<JoinResultRow> symmetricDifference() const
    {
        std::vector<JoinResultRow> result;

        auto a_it = table_a_.begin();
        auto b_it = table_b_.begin();

        while (a_it != table_a_.end() || b_it != table_b_.end())
        {
            if (b_it == table_b_.end() ||
                (a_it != table_a_.end() && a_it->first < b_it->first))
            {
                result.push_back({a_it->first, a_it->second, ""});
                ++a_it;
            }
            else if (a_it == table_a_.end() || b_it->first < a_it->first)
            {
                result.push_back({b_it->first, "", b_it->second});
                ++b_it;
            }
            else
            {
                ++a_it;
                ++b_it;
            }
        }

        return result;
    }

    static std::string formatResult(const std::vector<JoinResultRow>& records)
    {
        std::string output;

        for (const auto& record : records)
        {
            output += std::to_string(record.id) + "," +
                      record.a_name + "," +
                      record.b_name + "\n";
        }

        output += "OK\n";
        return output;
    }

    asio::strand<asio::io_context::executor_type> strand_;
    Table table_a_;
    Table table_b_;
};

class Session : public std::enable_shared_from_this<Session>
{
public:
    Session(tcp::socket socket, std::shared_ptr<JoinServer> server)
        : socket_(std::move(socket)),
          server_(std::move(server))
    {
    }

    void start()
    {
        readLine();
    }

private:
    void readLine()
    {
        auto self = shared_from_this();

        asio::async_read_until(socket_, input_, '\n',
            [this, self](ErrorCode error, std::size_t bytes_transferred)
            {
                if (error)
                {
                    return;
                }

                std::string received(
                    asio::buffers_begin(input_.data()),
                    asio::buffers_begin(input_.data()) + bytes_transferred);

                input_.consume(bytes_transferred);

                if (!received.empty() && received.back() == '\r')
                {
                    received.pop_back();
                }

                const std::string response = server_->handleCommand(received);
                write(response);
            });
    }

    void write(std::string response)
    {
        auto self = shared_from_this();

        outgoing_.push_back(std::move(response));

        if (writing_)
        {
            return;
        }

        writing_ = true;
        doWrite();
    }

    void doWrite()
    {
        auto self = shared_from_this();

        asio::async_write(socket_,
            asio::buffer(outgoing_.front()),
            [this, self](ErrorCode error, std::size_t)
            {
                outgoing_.pop_front();

                if (error)
                {
                    return;
                }

                if (!outgoing_.empty())
                {
                    doWrite();
                }
                else
                {
                    writing_ = false;
                    readLine();
                }
            });
    }

    tcp::socket socket_;
    asio::streambuf input_;
    std::deque<std::string> outgoing_;
    bool writing_ = false;
    std::shared_ptr<JoinServer> server_;
};

class Server
{
public:
    Server(asio::io_context& io_context, unsigned short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)),
          server_(std::make_shared<JoinServer>(io_context))
    {
        accept();
    }

private:
    void accept()
    {
        acceptor_.async_accept(
            [this](ErrorCode error, tcp::socket socket)
            {
                if (!error)
                {
                    std::make_shared<Session>(
                        std::move(socket),
                        server_)->start();
                }

                accept();
            });
    }

    tcp::acceptor acceptor_;
    std::shared_ptr<JoinServer> server_;
};

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <port>\n";
        return 1;
    }

    try
    {
        const unsigned short port =
            static_cast<unsigned short>(std::stoul(argv[1]));

        asio::io_context io_context;
        Server server(io_context, port);

        io_context.run();
    }
    catch (const std::exception& error)
    {
        std::cerr << "Error: " << error.what() << "\n";
        return 1;
    }

    return 0;
}