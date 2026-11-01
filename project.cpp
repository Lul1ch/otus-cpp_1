#include <boost/json.hpp>

#include <cmath>
#include <ctime>
#include <functional>
#include <map>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <set>

#include <chrono>
#include <iomanip>
#include <random>
#include <sstream>
#include <thread>

#include <condition_variable>
#include <deque>
#include <mutex>
#include <csignal>

#include <type_traits>

namespace json = boost::json;

namespace
{

    volatile std::sig_atomic_t stop_requested = 0;

    void handleStopSignal(int)
    {
        stop_requested = 1;
    }

}  

struct ConsumptionCoefficients
{
    double food_g_per_kg = 0.0;
    double water_notification_ml_per_kg = 0.0;
};

struct PetInfo
{
    std::string name;
    std::size_t age = 0;
    std::string gender;
    double weight = 0.0;
    double height = 0.0;
    bool is_sterilized = false;
    std::string species;

    double daily_food_limit_g = 0.0;
    double daily_water_notification_threshold_ml = 0.0;
};

struct Config
{
    std::string path_to_file;
    std::ifstream input_stream;
    std::ofstream output_stream;
};

enum ImportanceLevel
{
    INFO,
    WARNING,
    CRITICAL
};

enum class ActionType
{
    FOOD_CONSUMED,
    WATER_CONSUMED,
    ACTIVITY_DETECTED,
    FORBIDDEN_ZONE_ENTERED,

    FOOD_LIMIT_EXCEEDED,
    WATER_LIMIT_EXCEEDED
};

struct Action
{
    std::time_t timestamp = 0;
    ImportanceLevel level = ImportanceLevel::INFO;
    std::string pet_name;
    std::string event;

    ActionType type = ActionType::FOOD_CONSUMED;

    // FoodConsumed — граммы.
    // WaterConsumed — миллилитры.
    // ActivityDetected — минуты.
    double amount = 0.0;
};

void calculateDailyLimits(
    PetInfo& pet,
    const ConsumptionCoefficients& coefficients)
{
    if (!std::isfinite(pet.weight) || pet.weight <= 0.0)
    {
        throw std::invalid_argument(
            "Pet weight must be finite and positive");
    }

    if (!std::isfinite(coefficients.food_g_per_kg) ||
        coefficients.food_g_per_kg < 0.0 ||
        !std::isfinite(
            coefficients.water_notification_ml_per_kg) ||
        coefficients.water_notification_ml_per_kg < 0.0)
    {
        throw std::invalid_argument(
            "Consumption coefficients must be finite and nonnegative");
    }

    const double food_limit =
        pet.weight * coefficients.food_g_per_kg;

    const double water_threshold =
        pet.weight *
        coefficients.water_notification_ml_per_kg;

    if (!std::isfinite(food_limit) ||
        !std::isfinite(water_threshold))
    {
        throw std::overflow_error(
            "Calculated consumption values are too large");
    }

    pet.daily_food_limit_g = food_limit;
    pet.daily_water_notification_threshold_ml = water_threshold;
}

class IActionObserver
{
public:
    virtual ~IActionObserver() = default;

    virtual void onActionHappened(const Action& action) = 0;
};

class ActionProcessor
{
public:
    void subscribe(std::shared_ptr<IActionObserver> observer)
    {
        if (!observer)
        {
            throw std::invalid_argument(
                "Observer must not be null");
        }

        m_observers.push_back(std::move(observer));
    }

    void process(const Action& action)
    {
        for (const auto& observer : m_observers)
        {
            try
            {
                observer->onActionHappened(action);
            }
            catch (const std::exception& error)
            {
                std::cerr
                    << "Ошибка обработчика события: "
                    << error.what() << '\n';
            }
            catch (...)
            {
                std::cerr
                    << "Неизвестная ошибка обработчика события\n";
            }
        }
    }

    void process(const std::vector<Action>& actions)
    {
        for (const auto& action : actions)
        {
            process(action);
        }
    }

private:
    std::vector<std::shared_ptr<IActionObserver>> m_observers;
};

class AsyncActionLogger
{
public:
    explicit AsyncActionLogger(ActionProcessor& processor)
        : m_processor(processor)
    {
        m_worker = std::thread([this]
        {
            workerLoop();
        });
    }

    ~AsyncActionLogger()
    {
        stop();
    }

    AsyncActionLogger(const AsyncActionLogger&) = delete;
    AsyncActionLogger& operator=(const AsyncActionLogger&) = delete;

    // Предупреждения анализатора:
    // вывод через подписчиков ActionProcessor.
    void submit(std::vector<Action> actions)
    {
        if (actions.empty())
        {
            return;
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);

            checkNotStopping();

            // Проверяем сложение размеров на переполнение
            // и превышение предельного размера вектора.
            if (actions.size() > m_queue.max_size() - m_queue.size())
            {
                throw std::length_error(
                    "Too many actions in logger queue");
            }

            const std::size_t required_size =
                m_queue.size() + actions.size();

            // Все потенциальные выделения памяти для очереди —
            // до начала добавления новых событий.
            m_queue.reserve(required_size);

            for (auto& action : actions)
            {
                m_queue.push_back(LogEntry{
                    std::move(action),
                    false
                });
            }
        }

        m_condition.notify_one();
    }

    // Исходное событие устройства:
    // только отладочный вывод в консоль.
    void submitDeviceAction(Action action)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            checkNotStopping();

            m_queue.push_back(LogEntry{
                std::move(action),
                true
            });
        }

        m_condition.notify_one();
    }

    // Вызывать из управляющего потока.
    // Одновременные вызовы stop() не предусмотрены.
    void stop()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stopping = true;
        }

        m_condition.notify_all();

        if (m_worker.joinable())
        {
            m_worker.join();
        }
    }

private:
    struct LogEntry
    {
        Action action;
        bool device_message = false;
    };

    static_assert(
        std::is_nothrow_move_constructible_v<LogEntry>,
        "LogEntry must have a noexcept move constructor");

    // Вызывается только под m_mutex.
    void checkNotStopping() const
    {
        if (m_stopping)
        {
            throw std::runtime_error(
                "Cannot submit actions: logger is stopping");
        }
    }

    void workerLoop()
    {
        while (true)
        {
            std::vector<LogEntry> entries;

            {
                std::unique_lock<std::mutex> lock(m_mutex);

                m_condition.wait(lock, [this]
                {
                    return m_stopping || !m_queue.empty();
                });

                if (m_queue.empty())
                {
                    return;
                }

                entries.swap(m_queue);
            }

            // Мьютекс очереди уже отпущен.
            for (const auto& entry : entries)
            {
                try
                {
                    if (entry.device_message)
                    {
                        std::cout
                            << "[Устройство] "
                            << entry.action.event
                            << '\n';
                    }
                    else
                    {
                        m_processor.process(entry.action);
                    }
                }
                catch (const std::exception& error)
                {
                    std::cerr
                        << "Ошибка логирования: "
                        << error.what()
                        << '\n';
                }
                catch (...)
                {
                    std::cerr
                        << "Неизвестная ошибка логирования\n";
                }
            }
        }
    }

    ActionProcessor& m_processor;

    std::mutex m_mutex;
    std::condition_variable m_condition;

    std::vector<LogEntry> m_queue;
    bool m_stopping = false;

    std::thread m_worker;
};

class ConsoleLogger : public IActionObserver
{
public: 

    ~ConsoleLogger() override = default;

    void onActionHappened(const Action& action) override
    {
        std::cout << "Action - " << action.event << "\n";
    }
};

class FileLogger : public IActionObserver
{
public:

    ~FileLogger() override = default;

    void onActionHappened(const Action& action) override
    {
        const std::string file_path = "./action_" + action.pet_name + "_" + std::to_string(action.timestamp) + ".log";
        std::ofstream file(file_path, std::ios::app);

        if (!file.is_open())
        {
            std::cerr << "Не удалось открыть файл - " << file_path << "\n";
            return;
        }

        file << "Action - " << action.event << '\n';
        file.flush();

        if (!file)
        {
            throw std::runtime_error(
                "Не удалось записать лог: " + file_path);
        }
    }
};

class ConfigParser
{
public:

  std::vector<PetInfo> parsePetsFile(Config& config)
  {
    json::value parsed_file = json::parse(config.input_stream);

    json::object& root = parsed_file.as_object();

    json::array& pets_array = root.at("pets").as_array();

    std::vector<PetInfo> pets;

    for (const auto& pet_value : pets_array)
    {
        if (!pet_value.is_object())
        {
            throw std::runtime_error(
                "Every element in 'pets' must be an object");
        }

        pets.push_back(parsePet(pet_value.as_object()));
    }

    return pets;
  }

private:  
    PetInfo parsePet(const json::object& pet_obj)
    {
        PetInfo pet_info{};

        pet_info.name =
            json::value_to<std::string>(pet_obj.at("name"));

        const auto age = json::value_to<std::int64_t>(
            pet_obj.at("age"));

        if (age < 0)
        {
            throw std::runtime_error(
                "Pet age must not be negative: " + pet_info.name);
        }

        pet_info.age = static_cast<std::size_t>(age);

        pet_info.gender =
            json::value_to<std::string>(pet_obj.at("gender"));

        pet_info.weight =
            json::value_to<double>(pet_obj.at("weight"));

        pet_info.height =
            json::value_to<double>(pet_obj.at("height"));

        pet_info.is_sterilized =
            pet_obj.at("is_sterilized").as_bool();

        pet_info.species =
            json::value_to<std::string>(pet_obj.at("species"));

        if (pet_info.name.empty())
        {
            throw std::runtime_error("Pet name must not be empty");
        }

        if (!std::isfinite(pet_info.weight) ||
            pet_info.weight <= 0.0)
        {
            throw std::runtime_error(
                "Pet weight must be finite and positive: " +
                pet_info.name);
        }

        if (!std::isfinite(pet_info.height) ||
            pet_info.height < 0.0)
        {
            throw std::runtime_error(
                "Pet height must be finite and nonnegative: " +
                pet_info.name);
        }

        return pet_info;
    }
};

class ConfigProcessor
{
public:
    ConfigProcessor()
    {
        m_config_parser = std::make_unique<ConfigParser>();
    }

    std::vector<PetInfo> processConfig(
        const std::string& path_to_file)
    {
        Config config;
        config.path_to_file = path_to_file;
        config.input_stream.open(path_to_file);

        if (!config.input_stream.is_open())
        {
            throw std::runtime_error(
                "Не удалось открыть файл: " + path_to_file);
        }

        return m_config_parser->parsePetsFile(config);
    }

private:
    std::unique_ptr<ConfigParser> m_config_parser;
};

class PetsContextHolder
{
public:

    using DailyKey = std::pair<std::string, std::string>;

    std::vector<std::string> getPetNames() const
    {
        std::vector<std::string> names;
        names.reserve(m_pets.size());

        for (const auto& pet : m_pets)
        {
            names.push_back(pet.name);
        }

        return names;
    }

    void setPets(
        std::vector<PetInfo> pets,
        const ConsumptionCoefficients& coefficients)
    {
        std::set<std::string> names;

        for (auto& pet : pets)
        {
            if (pet.name.empty())
            {
                throw std::invalid_argument(
                    "Pet name must not be empty");
            }

            if (!names.insert(pet.name).second)
            {
                throw std::invalid_argument(
                    "Duplicate pet name: " + pet.name);
            }

            calculateDailyLimits(pet, coefficients);
        }

        m_pets = std::move(pets);

        // При полной замене набора питомцев сбрасываем старую статистику.
        m_daily_stats.clear();
    }

    const PetInfo* findPet(const std::string& name) const
    {
        for (const auto& pet : m_pets)
        {
            if (pet.name == name)
            {
                return &pet;
            }
        }

        return nullptr;
    }

    void printPets() const
    {
        for (const auto& pet : m_pets) {
            std::cout << "Name: " << pet.name << "\n";
            std::cout << "  Age: " << pet.age << " years\n";
            std::cout << "  Gender: " << pet.gender << "\n";
            std::cout << "  Weight: " << pet.weight << " kg\n";
            std::cout << "  Height: " << pet.height << " cm\n";
            std::cout << "  Sterilized: " << (pet.is_sterilized ? "yes" : "no") << "\n";
            std::cout << "  Species: " << pet.species << "\n";
            std::cout << "\n";
        }
    }

    double getPetFoodConsumed(const DailyKey& key) const
    {
        const auto it = m_daily_stats.find(key);

        return it == m_daily_stats.end()
            ? 0.0
            : it->second.food_g;
    }

    double getPetWaterConsumed(const DailyKey& key) const
    {
        const auto it = m_daily_stats.find(key);

        return it == m_daily_stats.end()
            ? 0.0
            : it->second.water_ml;
    }

    double updatePetFoodConsumed(
        const DailyKey& key,
        double food_g)
    {
        auto& stats = m_daily_stats[key];
        stats.food_g += food_g;
        return stats.food_g;
    }

    double updatePetWaterConsumed(
        const DailyKey& key,
        double water_ml)
    {
        auto& stats = m_daily_stats[key];
        stats.water_ml += water_ml;
        return stats.water_ml;
    }

    void savePetActivity(
        const DailyKey& key,
        double activity_minutes)
    {
        m_daily_stats[key].activity_minutes += activity_minutes;
    }

private:
    struct DailyStats
    {
        double food_g = 0.0;
        double water_ml = 0.0;
        double activity_minutes = 0.0;
    };

    std::map<
        std::pair<std::string, std::string>,
        DailyStats
    > m_daily_stats;

    std::vector<PetInfo> m_pets;
};

class ActionsAnalyzer
{
public:
    using DayKeyFunction =
        std::function<std::string(std::time_t)>;

    ActionsAnalyzer(
        PetsContextHolder& context_holder,
        DayKeyFunction day_key)
        : m_day_key(std::move(day_key)),
        m_context_holder(context_holder)
    {
        if (!m_day_key)
        {
            throw std::invalid_argument(
                "Day key function is required");
        }
    }

    void receiveAction(const Action& action)
    {
         const PetInfo* pet =
        m_context_holder.findPet(action.pet_name);

        if (!pet)
        {
            return;
        }

        switch (action.type)
        {
        case ActionType::FOOD_CONSUMED:
        case ActionType::WATER_CONSUMED:
        case ActionType::ACTIVITY_DETECTED:
            if (!std::isfinite(action.amount) ||
                action.amount <= 0.0)
            {
                throw std::invalid_argument(
                    "Action amount must be finite and positive");
            }
            break;

        case ActionType::FORBIDDEN_ZONE_ENTERED:
            // Для входа в зону amount не требуется.
            break;

        default:
            // Сгенерированные предупреждения не анализируем повторно.
            return;
        }

        switch (action.type)
        {
        case ActionType::FOOD_CONSUMED:
            analyzeFood(action, *pet);
            break;

        case ActionType::WATER_CONSUMED:
            analyzeWater(action, *pet);
            break;

        case ActionType::FORBIDDEN_ZONE_ENTERED:
            warnForbiddenZone(action);
            break;

        case ActionType::ACTIVITY_DETECTED:
        {
            const auto key = std::make_pair(
                action.pet_name,
                m_day_key(action.timestamp));

            m_context_holder.savePetActivity(
                key,
                action.amount);
            break;
        }

        default:
            break;
        }
    }

    bool isAnyActionsToLog() const
    {
        return !m_actions_to_log.empty();
    }

    std::vector<Action> takeActionsToLog()
    {
        std::vector<Action> result;
        result.swap(m_actions_to_log);
        return result;
    }

private:

    void analyzeFood(
        const Action& action,
        const PetInfo& pet)
    {
        const auto key = std::make_pair(
            action.pet_name,
            m_day_key(action.timestamp));

        const double previous =
            m_context_holder.getPetFoodConsumed(key);

        const double total =
            m_context_holder.updatePetFoodConsumed(
                key,
                action.amount);

        if (pet.daily_food_limit_g <= 0.0 ||
            previous > pet.daily_food_limit_g ||
            total <= pet.daily_food_limit_g)
        {
            return;
        }

        addWarning(
            action,
            ImportanceLevel::WARNING,
            ActionType::FOOD_LIMIT_EXCEEDED,
            total,
            action.pet_name +
                " превысил установленный суточный лимит корма: " +
                std::to_string(total) + " г из " +
                std::to_string(pet.daily_food_limit_g) + " г");
    }

    void analyzeWater(
        const Action& action,
        const PetInfo& pet)
    {
        const auto key = std::make_pair(
            action.pet_name,
            m_day_key(action.timestamp));

        const double previous =
            m_context_holder.getPetWaterConsumed(key);

        const double total =
            m_context_holder.updatePetWaterConsumed(
                key,
                action.amount);

        const double threshold =
            pet.daily_water_notification_threshold_ml;

        if (threshold <= 0.0 ||
            previous > threshold ||
            total <= threshold)
        {
            return;
        }

        addWarning(
            action,
            ImportanceLevel::WARNING,
            ActionType::WATER_LIMIT_EXCEEDED,
            total,
            action.pet_name +
                " превысил настроенный суточный порог питья: " +
                std::to_string(total) + " мл из " +
                std::to_string(threshold) + " мл");
    }

    void warnForbiddenZone(const Action& action)
    {
        addWarning(
            action,
            ImportanceLevel::WARNING,
            ActionType::FORBIDDEN_ZONE_ENTERED,
            0.0,
            action.pet_name + " вошёл в запрещённую зону");
    }

    void addWarning(
        const Action& source,
        ImportanceLevel message_level,
        ActionType type,
        double total,
        std::string message)
    {
        m_actions_to_log.push_back(Action{
            source.timestamp,
            message_level,
            source.pet_name,
            std::move(message),
            type,
            total
        });
    }

    DayKeyFunction m_day_key;

    std::vector<Action> m_actions_to_log;
    PetsContextHolder& m_context_holder;
};

class RandomActionGenerator
{
public:
    RandomActionGenerator(
        std::vector<std::string> pet_names,
        ActionsAnalyzer& analyzer,
        AsyncActionLogger& logger)
        : m_pet_names(std::move(pet_names)),
        m_analyzer(analyzer),
        m_logger(logger),
        m_engine(std::random_device{}())
    {
        if (m_pet_names.empty())
        {
            throw std::invalid_argument(
                "Cannot generate actions: pet list is empty");
        }
    }

    void generateAndSend()
    {
        std::uniform_int_distribution<std::size_t> pet_distribution(
            0,
            m_pet_names.size() - 1);

        std::uniform_int_distribution<int> type_distribution(0, 3);

        Action action{};
        action.timestamp = std::time(nullptr);
        action.level = ImportanceLevel::INFO;
        action.pet_name =
            m_pet_names[pet_distribution(m_engine)];

        switch (type_distribution(m_engine))
        {
        case 0:
            action.type = ActionType::FOOD_CONSUMED;
            action.amount = randomAmount(5, 30);
            action.event =
                action.pet_name + " съел " +
                std::to_string(static_cast<int>(action.amount)) +
                " г корма";
            break;

        case 1:
            action.type = ActionType::WATER_CONSUMED;
            action.amount = randomAmount(10, 80);
            action.event =
                action.pet_name + " выпил " +
                std::to_string(static_cast<int>(action.amount)) +
                " мл воды";
            break;

        case 2:
            action.type = ActionType::ACTIVITY_DETECTED;
            action.amount = randomAmount(1, 15);
            action.event =
                action.pet_name + " активно двигался " +
                std::to_string(static_cast<int>(action.amount)) +
                " мин";
            break;

        case 3:
            action.type = ActionType::FORBIDDEN_ZONE_ENTERED;
            action.amount = 0.0;
            action.event =
                action.pet_name + " вошёл в запрещённую зону";
            break;
        }

        // Для отладки показываем исходное событие отдельно.
        m_logger.submitDeviceAction(action);

        m_analyzer.receiveAction(action);
    }

private:
    int randomAmount(int minimum, int maximum)
    {
        std::uniform_int_distribution<int> distribution(
            minimum,
            maximum);

        return distribution(m_engine);
    }

    std::vector<std::string> m_pet_names;
    ActionsAnalyzer& m_analyzer;
    AsyncActionLogger& m_logger;
    std::mt19937 m_engine;
};

int main()
{
    try
    {
        std::signal(SIGINT, handleStopSignal);
        std::signal(SIGTERM, handleStopSignal);

        ConfigProcessor config_processor;

        PetsContextHolder context_holder;

        // Только демонстрационные коэффициенты.
        // Не использовать как реальные рекомендации по уходу.
        const ConsumptionCoefficients coefficients{
            15.0,  // граммов корма на кг веса
            60.0   // мл на кг веса для порога уведомления
        };

        context_holder.setPets(
            config_processor.processConfig("./config.json"),
            coefficients);

        context_holder.printPets();

        ActionProcessor action_processor;
        action_processor.subscribe(
            std::make_shared<ConsoleLogger>());
        
        action_processor.subscribe(
            std::make_shared<FileLogger>());

        AsyncActionLogger async_logger(action_processor);

        auto day_key = [](std::time_t timestamp)
        {
            // Используем локальный часовой пояс системы.
            const std::tm* local = std::localtime(&timestamp);

            if (!local)
            {
                throw std::runtime_error(
                    "Cannot convert action timestamp");
            }

            std::ostringstream output;
            output << std::put_time(local, "%Y-%m-%d");
            return output.str();
        };

        ActionsAnalyzer actions_analyzer(
            context_holder,
            day_key);

        RandomActionGenerator generator(
            context_holder.getPetNames(),
            actions_analyzer,
            async_logger);

        while (!stop_requested)
        {
            generator.generateAndSend();

            async_logger.submit(
                actions_analyzer.takeActionsToLog());

            std::this_thread::sleep_for(
                std::chrono::seconds(1));
        }

        async_logger.stop();
    }
    catch (const std::exception& error)
    {
        std::cerr << "Ошибка: " << error.what() << '\n';
        return 1;
    }
}