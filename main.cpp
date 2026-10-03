#include "async.h"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

std::string makeCommand(int number)
{
    return "cmd" + std::to_string(number);
}

void sendChunk(void* context, const std::string& chunk)
{
    receive(context, chunk.data(), chunk.size());
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <bulk_size>\n";
        return 1;
    }

    const std::size_t bulk_size =
        static_cast<std::size_t>(std::stoull(argv[1]));

    void* context1 = connect(bulk_size);
    void* context2 = connect(bulk_size);

    std::string data1;
    std::string data2;

    for (int i = 1; i <= 100000; ++i)
    {
        data1 += makeCommand(i) + "\n";
        data2 += makeCommand(i + 100000) + "\n";
    }

    data1 += "{\n";
    for (int i = 200001; i <= 200010; ++i)
    {
        data1 += makeCommand(i) + "\n";
    }
    data1 += "}\n";
    data1 += makeCommand(300001) + "\n";

    // Подаём данные порциями по 64 байта.
    // Строки и даже команды могут разрываться между вызовами receive().
    constexpr std::size_t chunk_size = 64;

    for (std::size_t i = 0; i < data1.size(); i += chunk_size)
    {
        sendChunk(context1, data1.substr(i, chunk_size));
    }

    for (std::size_t i = 0; i < data2.size(); i += chunk_size)
    {
        sendChunk(context2, data2.substr(i, chunk_size));
    }

    disconnect(context1);
    disconnect(context2);

    // Даём фоновым потокам возможность завершить вывод.
    std::this_thread::sleep_for(std::chrono::seconds(1));

    return 0;
}