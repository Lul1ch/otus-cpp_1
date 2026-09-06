#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <ctime>
#include <chrono>
#include <utility>
#include <thread>

struct Bulk
{
    std::time_t timestamp;
    std::vector<std::string> commands;
};

class IBulkObserver
{
public:
    virtual ~IBulkObserver() = default;

    virtual void onBulkReady(const Bulk& bulk) = 0;
};

class BulkProcessor
{
public:

    void subscribe(std::shared_ptr<IBulkObserver> observer)
    {
        m_observers.push_back(std::move(observer));
    }

    void process(const Bulk& bulk)
    {
        for(auto& observer : m_observers)
        {
            observer->onBulkReady(bulk);
        }
    }

private:

    std::vector<std::shared_ptr<IBulkObserver>> m_observers;
};

class ConsoleLogger : public IBulkObserver
{
public: 

    ~ConsoleLogger() override = default;

    void onBulkReady(const Bulk& bulk) override
    {
        std::cout << "bulk: ";
        bool is_first_element = true;
        for(const auto& command : bulk.commands)
        {
            if (!is_first_element)
            {
                std::cout << ", ";
            }
            is_first_element = false;

            std::cout << command;
        }
        std::cout << "\n";
    }
};

class FileLogger : public IBulkObserver
{
public:

    ~FileLogger() override = default;

    void onBulkReady(const Bulk& bulk) override
    {
        std::string file_path = std::string("./bulk" + std::to_string(bulk.timestamp) + ".log");
        std::ofstream file(file_path);

        if (!file.is_open())
        {
            std::cerr << "Не удалось открыть файл - " << file_path << "\n";
            return;
        }

        file << "bulk: ";
        bool is_first_element = true;
        for(const auto& command : bulk.commands)
        {
            if (!is_first_element)
            {
                file << ", ";
            }
            is_first_element = false;

            file << command;
        }
        file << "\n";
    }
};

class BulkCreator
{
public:

    Bulk makeBulk(std::time_t cur_timestamp)
    {
        Bulk bulk;
        for(auto cmd : m_current_commands)
        {
            bulk.commands.push_back(cmd);
        }
        m_current_commands.clear();

        bulk.timestamp = cur_timestamp;
        return bulk;
    }

    void saveCmd(std::string cmd)
    {
        m_current_commands.push_back(cmd);  
    }

private:

    std::vector<std::string> m_current_commands;
};

class LogicHolder
{
public:

    void increaseOpenBracketsNumber()
    {
        m_open_brackets_number++;
    }

    void decreaseOpenBracketsNumber()
    {
        if (m_open_brackets_number == 0)
        {
            return;
        }

        m_open_brackets_number--;
    }

    bool isNoOpenBrackets()
    {
        return m_open_brackets_number == 0;
    }

    bool isDynamicBlockJustOpened()
    {
        return m_open_brackets_number == 1;
    }

    void increaseCmdsNumber()
    {
        m_cmds_number++;
    }

    void clearCmdsNumber()
    {
        m_cmds_number = 0;
    }

    bool isCmdsReachCapacity()
    {
        return m_cmds_capacity <= m_cmds_number;
    }

    bool isCmdsNumberNotNull()
    {
        return m_cmds_number != 0;
    }

    size_t getCmdsNumber()
    {
        return m_cmds_number;
    }

    void setCmdsCapacityLevel(size_t number)
    {
        m_cmds_capacity = number;
    }

private:
    size_t m_cmds_capacity = 0;
    size_t m_cmds_number = 0;
    size_t m_open_brackets_number = 0;
};

void completeBulkProcessing(std::unique_ptr<LogicHolder>& logic_holder, std::unique_ptr<BulkCreator>& bulk_creator, std::unique_ptr<BulkProcessor>& bulk_processor, std::time_t& time)
{
    Bulk bulk = bulk_creator->makeBulk(time);

    bulk_processor->process(bulk);

    logic_holder->clearCmdsNumber();
    time = 0;
}

int main(int argc, char* argv[])
{
    std::unique_ptr<BulkProcessor> bulk_processor = std::make_unique<BulkProcessor>();
    bulk_processor->subscribe(std::make_shared<ConsoleLogger>());
    bulk_processor->subscribe(std::make_shared<FileLogger>());

    std::unique_ptr<BulkCreator> bulk_creator = std::make_unique<BulkCreator>();
    std::unique_ptr<LogicHolder> logic_holder = std::make_unique<LogicHolder>();

    if (argc < 2) 
    {
        std::cerr << "Использование: " << argv[0] << " <размер блока>\n";
        return 1;
    }

    logic_holder->setCmdsCapacityLevel(static_cast<std::size_t>(std::stoull(argv[1])));

    std::string line;
    std::time_t time = 0;
    while (std::getline(std::cin, line)) 
    {
        if(line == "EOF")
        {
            break;
        }

        if (line == "{")
        {
            logic_holder->increaseOpenBracketsNumber();

            if (logic_holder->isDynamicBlockJustOpened() && logic_holder->isCmdsNumberNotNull())
            {
                completeBulkProcessing(logic_holder, bulk_creator, bulk_processor, time);
            }
        }
        else if (line == "}")
        {
            logic_holder->decreaseOpenBracketsNumber();

            if (logic_holder->isNoOpenBrackets())
            {
                completeBulkProcessing(logic_holder, bulk_creator, bulk_processor, time);
            }
        }
        else
        {
            if (time == 0)
            {
                auto now = std::chrono::system_clock::now();
                time = std::chrono::system_clock::to_time_t(now);

                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            logic_holder->increaseCmdsNumber();
            bulk_creator->saveCmd(line);

            if (logic_holder->isCmdsReachCapacity() && logic_holder->isNoOpenBrackets())
            {
                completeBulkProcessing(logic_holder, bulk_creator, bulk_processor, time);
            }
        }
    }

    if (logic_holder->getCmdsNumber() > 0 && logic_holder->isNoOpenBrackets())
    {
        Bulk bulk = bulk_creator->makeBulk(time);

        bulk_processor->process(bulk);
    }
}